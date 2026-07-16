import { cleanText, cleanUid, logEvent, touchLocker, type LockerSession } from '@/lib/locker';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const deviceId = cleanText(searchParams.get('deviceId') || searchParams.get('device_id') || 'locker-01');

    if (!deviceId) {
      return Response.json({ ok: false, error: 'Missing deviceId' }, { status: 400 });
    }

    await touchLocker(deviceId);

    const { data, error } = await supabase
      .from('locker_sessions')
      .select('uid, locker_number')
      .eq('device_id', deviceId)
      .eq('is_active', true)
      .order('locker_number', { ascending: true });

    if (error) {
      return Response.json({ ok: false, error: error.message }, { status: 500 });
    }

    return Response.json({
      ok: true,
      deviceId,
      lockers: (data || []).map((session) => ({
        locker: session.locker_number,
        uid: session.uid,
      })),
    });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const deviceId = cleanText(body.deviceId || body.device_id || 'locker-01');
    const localLockers = normalizeLocalLockers(body.lockers);

    if (!deviceId) {
      return Response.json({ ok: false, error: 'Missing deviceId' }, { status: 400 });
    }

    await touchLocker(deviceId);

    const { data: sessions, error } = await supabase
      .from('locker_sessions')
      .select('*')
      .eq('device_id', deviceId)
      .eq('is_active', true);

    if (error) {
      return Response.json({ ok: false, error: error.message }, { status: 500 });
    }

    const now = new Date().toISOString();
    const localByLocker = new Map(localLockers.map((locker) => [locker.locker, locker.uid]));
    const activeSessions = (sessions || []) as LockerSession[];
    const stillActive = new Set<string>();

    for (const session of activeSessions) {
      const localUid = localByLocker.get(session.locker_number) || '';
      if (localUid === session.uid) {
        stillActive.add(lockerKey(session.locker_number, session.uid));
        continue;
      }

      const { error: updateError } = await supabase
        .from('locker_sessions')
        .update({
          is_active: false,
          picked_up_at: now,
          updated_at: now,
        })
        .eq('id', session.id);

      if (updateError) {
        return Response.json({ ok: false, error: updateError.message }, { status: 500 });
      }

      await logEvent('sync_pickup', {
        device_id: deviceId,
        uid: session.uid,
        locker_number: session.locker_number,
        session_id: session.id,
        payload: { source: 'esp_state', localUid: localUid || null },
      });
    }

    for (const locker of localLockers) {
      if (stillActive.has(lockerKey(locker.locker, locker.uid))) continue;

      const { data, error: insertError } = await supabase
        .from('locker_sessions')
        .insert({
          device_id: deviceId,
          uid: locker.uid,
          locker_number: locker.locker,
          deposited_at: now,
          is_active: true,
          payment_status: 'none',
          fee_amount: 0,
        })
        .select('id')
        .single();

      if (insertError) {
        return Response.json({ ok: false, error: insertError.message }, { status: 409 });
      }

      await logEvent('sync_deposit', {
        device_id: deviceId,
        uid: locker.uid,
        locker_number: locker.locker,
        session_id: data.id,
        payload: { source: 'esp_state' },
      });
    }

    return Response.json({ ok: true, deviceId, lockers: localLockers });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}

function normalizeLocalLockers(value: unknown) {
  if (!Array.isArray(value)) return [];

  const byLocker = new Map<number, string>();
  for (const item of value) {
    const raw = item as { locker?: unknown; uid?: unknown };
    const locker = Number(raw.locker);
    const uid = cleanUid(raw.uid);

    if (!Number.isInteger(locker) || locker < 1 || !uid) continue;
    byLocker.set(locker, uid);
  }

  return Array.from(byLocker, ([locker, uid]) => ({ locker, uid })).sort((a, b) => a.locker - b.locker);
}

function lockerKey(locker: number, uid: string) {
  return `${locker}:${uid}`;
}
