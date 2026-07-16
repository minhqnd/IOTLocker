import { isPaymentSettled, type LockerSession } from '@/lib/locker';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const paymentId = (searchParams.get('paymentId') || searchParams.get('payment_id') || '').trim();

    if (!paymentId) {
      return Response.json({ ok: false, error: 'Missing paymentId' }, { status: 400 });
    }

    const { data: session, error } = await supabase
      .from('locker_sessions')
      .select('id, payment_status, payment_method, locker_number')
      .eq('payment_id', paymentId)
      .maybeSingle();

    if (error) {
      return Response.json({ ok: false, error: error.message }, { status: 500 });
    }

    if (!session) {
      return Response.json({ ok: false, error: 'Payment not found' }, { status: 404 });
    }

    const paid = isPaymentSettled(session.payment_status as LockerSession['payment_status']);
    return Response.json({
      ok: true,
      paid,
      allowOpen: paid,
      locker: session.locker_number,
      paymentStatus: session.payment_status,
      paymentMethod: session.payment_method,
    });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
