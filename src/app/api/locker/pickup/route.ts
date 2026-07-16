import { cleanText, cleanUid, findActiveSession, logEvent, touchLocker } from '@/lib/locker';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const deviceId = cleanText(body.deviceId || body.device_id || 'locker-01');
    const uid = cleanUid(body.uid);

    if (!deviceId || !uid) {
      return Response.json({ ok: false, error: 'Missing deviceId or uid' }, { status: 400 });
    }

    await touchLocker(deviceId);

    const { data: session, error } = await findActiveSession(deviceId, uid);
    if (error) {
      return Response.json({ ok: false, error: error.message }, { status: 500 });
    }

    if (!session) {
      return Response.json({ ok: false, error: 'Active session not found' }, { status: 404 });
    }

    const now = new Date().toISOString();
    const { error: updateError } = await supabase
      .from('locker_sessions')
      .update({
        is_active: false,
        picked_up_at: body.pickedUpAt || body.picked_up_at || now,
        updated_at: now,
      })
      .eq('id', session.id);

    if (updateError) {
      return Response.json({ ok: false, error: updateError.message }, { status: 500 });
    }

    await logEvent('pickup', {
      device_id: deviceId,
      uid,
      locker_number: session.locker_number,
      session_id: session.id,
      payload: body,
    });

    return Response.json({ ok: true });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
