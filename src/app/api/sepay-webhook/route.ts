import { extractOrderCode } from '@/lib/sepay';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function POST(request: Request) {
  try {
    const payload = await request.json();
    const content = String(payload.content || payload.description || '');
    const transferAmount = Number(payload.transferAmount || 0);
    const transferType = String(payload.transferType || '').toLowerCase();
    const sepayId = String(payload.id || '');
    const paymentId = extractOrderCode(payload.code || content);

    if (transferType && transferType !== 'in') {
      return Response.json({ success: true });
    }

    if (!paymentId) {
      return Response.json({ success: true });
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
      return Response.json({ success: true });
    }

    if (session.payment_status === 'paid') {
      return Response.json({ success: true });
    }

    if (transferAmount < Number(session.fee_amount || 0)) {
      return Response.json({ success: true });
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
      payload: { sepayId, transferAmount, transferType, content, paymentId },
    });

    return Response.json({ success: true });
  } catch (error: unknown) {
    return Response.json({ success: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
