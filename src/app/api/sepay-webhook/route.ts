import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';
import { extractOrderCode } from '@/lib/sepay';

export async function POST(request: Request) {
  try {
    const payload = await request.json();
    const { id, transferAmount, content } = payload;

    if (!id || typeof transferAmount !== 'number' || !content) {
      return NextResponse.json({ success: false, error: 'Invalid webhook payload parameters' }, { status: 400 });
    }

    // 1. Extract the 6-digit order code from bank transfer memo
    const orderCode = extractOrderCode(content);
    if (!orderCode) {
      console.log(`[SePay Webhook] Transaction ID: ${id} ignored. Reason: No 6-digit order code found in: "${content}"`);
      return NextResponse.json({ success: true, message: 'Ignored: No 6-digit order code in content description' });
    }

    // 2. Fetch the corresponding order
    const { data: order, error: fetchError } = await supabase
      .from('orders')
      .select('*')
      .eq('order_code', orderCode)
      .maybeSingle();

    if (fetchError) {
      return NextResponse.json({ success: false, error: fetchError.message }, { status: 500 });
    }

    if (!order) {
      console.log(`[SePay Webhook] Transaction ID: ${id} ignored. Reason: Extracted order code "${orderCode}" does not exist in DB.`);
      return NextResponse.json({ success: true, message: 'Ignored: Extracted order code not found in database' });
    }

    // 3. Idempotency: check if order is already marked as paid
    if (order.paid || order.status === 'paid' || order.status === 'picked') {
      return NextResponse.json({ success: true, message: 'Success: Order is already marked as paid' });
    }

    // Ensure we do not process the same transaction ID twice
    if (order.sepay_ref === String(id)) {
      return NextResponse.json({ success: true, message: 'Success: Webhook transaction already processed' });
    }

    // 4. Verification: check that the transfer amount matches the order amount
    if (transferAmount !== order.amount) {
      console.warn(`[SePay Webhook] Transaction ID: ${id} payment failed. Amount mismatch: transferAmount=${transferAmount}, expected=${order.amount}`);
      return NextResponse.json({ success: true, message: `Ignored: Amount mismatch. Received ${transferAmount}, expected ${order.amount}` });
    }

    // 5. Update the order status to paid
    const { error: updateError } = await supabase
      .from('orders')
      .update({
        paid: true,
        status: 'paid',
        sepay_ref: String(id),
        updated_at: new Date().toISOString(),
      })
      .eq('id', order.id);

    if (updateError) {
      return NextResponse.json({ success: false, error: updateError.message }, { status: 500 });
    }

    // 6. Log the payment event
    await supabase.from('events').insert({
      type: 'payment',
      locker_id: order.locker_id,
      compartment: order.compartment,
      order_code: orderCode,
      payload: {
        sepay_transaction_id: id,
        transfer_amount: transferAmount,
        content: content,
        previous_status: order.status,
        new_status: 'paid',
      },
    });

    return NextResponse.json({ success: true });
  } catch (error: any) {
    return NextResponse.json({ success: false, error: error.message || 'Internal Server Error' }, { status: 500 });
  }
}
