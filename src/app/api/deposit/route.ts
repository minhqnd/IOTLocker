import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { locker_id, order_code, compartment, pin } = body;

    if (!locker_id || !order_code || !compartment || !pin) {
      return NextResponse.json({ error: 'Missing required parameters' }, { status: 400 });
    }

    const cleanOrderCode = String(order_code).trim();
    const cleanPin = String(pin).trim();

    // 1. Fetch the order details
    const { data: order, error: fetchError } = await supabase
      .from('orders')
      .select('*')
      .eq('order_code', cleanOrderCode)
      .maybeSingle();

    if (fetchError) {
      return NextResponse.json({ error: fetchError.message }, { status: 500 });
    }

    if (!order) {
      return NextResponse.json({ error: `Order with code ${cleanOrderCode} not found` }, { status: 404 });
    }

    // Ensure order is not already processed or picked up
    if (order.status !== 'created') {
      return NextResponse.json({ error: `Order is already in state: ${order.status}` }, { status: 400 });
    }

    // Prepaid orders (is_cod = false) go to 'stored' (ready for customer pickup).
    // COD orders (is_cod = true) go to 'awaiting_payment' (requires payment before customer pickup).
    const targetStatus = order.is_cod ? 'awaiting_payment' : 'stored';

    // 2. Update order details
    const { error: updateError } = await supabase
      .from('orders')
      .update({
        locker_id,
        compartment,
        pin: cleanPin,
        status: targetStatus,
        updated_at: new Date().toISOString(),
      })
      .eq('id', order.id);

    if (updateError) {
      return NextResponse.json({ error: updateError.message }, { status: 500 });
    }

    // 3. Log event history
    await supabase.from('events').insert({
      type: 'deposit',
      locker_id,
      compartment,
      order_code: cleanOrderCode,
      payload: {
        pin: cleanPin,
        previous_status: 'created',
        new_status: targetStatus,
      },
    });

    return NextResponse.json({
      ok: true,
      is_cod: order.is_cod,
      amount: order.amount,
    });
  } catch (error: any) {
    return NextResponse.json({ error: error.message || 'Internal Server Error' }, { status: 500 });
  }
}
