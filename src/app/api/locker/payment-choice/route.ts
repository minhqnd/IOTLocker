import { cleanText, cleanUid, logEvent } from '@/lib/locker';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const deviceId = cleanText(body.deviceId || body.device_id || 'locker-01');
    const uid = cleanUid(body.uid);
    const paymentId = cleanText(body.paymentId || body.payment_id);
    const choice = cleanText(body.choice).toLowerCase();

    if (!deviceId || !uid || !paymentId || choice !== 'parking') {
      return Response.json({ ok: false, error: 'Invalid payment choice' }, { status: 400 });
    }

    const [{ data: session, error: sessionError }, { data: parkingCard, error: cardError }] = await Promise.all([
      supabase
        .from('locker_sessions')
        .select('*')
        .eq('device_id', deviceId)
        .eq('uid', uid)
        .eq('payment_id', paymentId)
        .eq('is_active', true)
        .maybeSingle(),
      supabase.from('parking_cards').select('uid, active').eq('uid', uid).eq('active', true).maybeSingle(),
    ]);

    const error = sessionError || cardError;
    if (error) return Response.json({ ok: false, error: error.message }, { status: 500 });
    if (!session) return Response.json({ ok: true, found: false, allowOpen: false, code: 'SESSION_NOT_FOUND' });
    if (!parkingCard) return Response.json({ ok: true, eligible: false, allowOpen: false, code: 'NOT_PARKING_CARD' });

    const now = new Date().toISOString();
    const { error: updateError } = await supabase
      .from('locker_sessions')
      .update({ payment_status: 'deferred', payment_method: 'parking', updated_at: now })
      .eq('id', session.id);

    if (updateError) return Response.json({ ok: false, error: updateError.message }, { status: 500 });

    await logEvent('parking_deferred', {
      device_id: deviceId,
      uid,
      locker_number: session.locker_number,
      session_id: session.id,
      payload: { paymentId, amount: session.fee_amount },
    });

    return Response.json({ ok: true, eligible: true, allowOpen: true, paymentStatus: 'deferred' });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
