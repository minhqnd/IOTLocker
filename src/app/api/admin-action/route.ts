import { cleanText, ensurePayment, logEvent, touchLocker } from '@/lib/locker';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

const DEMO_DEVICE_ID = 'locker-01';
const OVERDUE_MINUTES = 45;

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const action = cleanText(body.action);
    const deviceId = cleanText(body.deviceId || body.device_id || DEMO_DEVICE_ID);
    const locker = Number(body.locker);

    if (!deviceId || !Number.isInteger(locker) || locker < 1) {
      return Response.json({ ok: false, error: 'Missing deviceId or locker' }, { status: 400 });
    }

    await touchLocker(deviceId);

    if (action === 'deposit') return createDemoSession(deviceId, locker);

    const { data: session, error } = await supabase
      .from('locker_sessions')
      .select('*')
      .eq('device_id', deviceId)
      .eq('locker_number', locker)
      .eq('is_active', true)
      .maybeSingle();

    if (error) return Response.json({ ok: false, error: error.message }, { status: 500 });
    if (!session) return Response.json({ ok: false, error: 'Active session not found' }, { status: 404 });

    if (action === 'overdue') return updateSession(session.id, { deposited_at: minutesAgo(OVERDUE_MINUTES), payment_status: 'none', fee_amount: 0 }, action, deviceId, locker);
    if (action === 'paid') return updateSession(session.id, { payment_status: 'paid', paid_at: new Date().toISOString() }, action, deviceId, locker);
    if (action === 'pending') {
      const payableSession = await ensurePayment(session);
      await logEvent('admin_pending', { device_id: deviceId, uid: session.uid, locker_number: locker, session_id: session.id });
      return Response.json({ ok: true, paymentId: payableSession.payment_id });
    }
    if (action === 'pickup') return updateSession(session.id, { is_active: false, picked_up_at: new Date().toISOString() }, action, deviceId, locker);

    return Response.json({ ok: false, error: 'Unknown action' }, { status: 400 });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}

async function createDemoSession(deviceId: string, locker: number) {
  const now = new Date().toISOString();
  const uid = `DEMO${locker}${Math.floor(1000 + Math.random() * 9000)}`;
  const { data, error } = await supabase
    .from('locker_sessions')
    .insert({
      device_id: deviceId,
      uid,
      locker_number: locker,
      deposited_at: now,
      is_active: true,
      payment_status: 'none',
      fee_amount: 0,
    })
    .select()
    .single();

  if (error) return Response.json({ ok: false, error: error.message }, { status: 409 });
  await logEvent('admin_deposit', { device_id: deviceId, uid, locker_number: locker, session_id: data.id });
  return Response.json({ ok: true, sessionId: data.id });
}

async function updateSession(
  sessionId: string,
  patch: Record<string, unknown>,
  action: string,
  deviceId: string,
  locker: number
) {
  const now = new Date().toISOString();
  const { data, error } = await supabase
    .from('locker_sessions')
    .update({ ...patch, updated_at: now })
    .eq('id', sessionId)
    .select('uid')
    .single();

  if (error) return Response.json({ ok: false, error: error.message }, { status: 500 });
  await logEvent(`admin_${action}`, { device_id: deviceId, uid: data.uid, locker_number: locker, session_id: sessionId });
  return Response.json({ ok: true });
}

function minutesAgo(minutes: number) {
  return new Date(Date.now() - minutes * 60 * 1000).toISOString();
}
