import { cleanText, cleanUid, logEvent, touchLocker } from '@/lib/locker';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const deviceId = cleanText(body.deviceId || body.device_id || 'locker-01');
    const uid = cleanUid(body.uid);
    const locker = Number(body.locker);

    if (!deviceId || !uid || !Number.isInteger(locker) || locker < 1) {
      return Response.json({ ok: false, error: 'Missing deviceId, uid, or locker' }, { status: 400 });
    }

    await touchLocker(deviceId);

    const now = new Date().toISOString();
    const { data, error } = await supabase
      .from('locker_sessions')
      .insert({
        device_id: deviceId,
        uid,
        locker_number: locker,
        deposited_at: body.depositedAt || body.deposited_at || now,
        is_active: true,
        payment_status: 'none',
        fee_amount: 0,
      })
      .select()
      .single();

    if (error) {
      return Response.json({ ok: false, error: error.message }, { status: 409 });
    }

    await logEvent('deposit', {
      device_id: deviceId,
      uid,
      locker_number: locker,
      session_id: data.id,
      payload: body,
    });

    return Response.json({ ok: true, sessionId: data.id });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
