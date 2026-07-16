import { extractOrderCode } from '@/lib/sepay';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function POST(request: Request) {
  try {
    const payload = await request.json();
    const content = String(payload.content || payload.description || '');
    const transferAmount = Number(payload.transferAmount || payload.amount || 0);
    const sepayId = String(payload.id || payload.transactionId || '');
    const paymentId = extractOrderCode(content);

    if (!paymentId) {
      return Response.json({ success: true, message: 'Ignored: no payment id in content' });
    }

    const { data: session, error } = await supabase
      .from('locker_sessions')
      .select('*')
      .eq('payment_id', paymentId)
      .eq('is_active', true)
      .maybeSingle();

    if (error) {
      return Response.json({ success: false, error: error.message }, { status: 500 });
    }

    if (!session) {
      return Response.json({ success: true, message: 'Ignored: payment session not found' });
    }

    if (session.payment_status === 'paid') {
      return Response.json({ success: true, message: 'Already paid' });
    }

    if (transferAmount < Number(session.fee_amount || 0)) {
      return Response.json({ success: true, message: 'Ignored: amount too small' });
    }

    const now = new Date().toISOString();
    const { error: updateError } = await supabase
      .from('locker_sessions')
      .update({
        payment_status: 'paid',
        paid_at: now,
        updated_at: now,
      })
      .eq('id', session.id);

    if (updateError) {
      return Response.json({ success: false, error: updateError.message }, { status: 500 });
    }

    await supabase.from('events').insert({
      type: 'payment',
      device_id: session.device_id,
      uid: session.uid,
      locker_number: session.locker_number,
      session_id: session.id,
      payload: { sepayId, transferAmount, content, paymentId },
    });

    return Response.json({ success: true });
  } catch (error: unknown) {
    return Response.json({ success: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
