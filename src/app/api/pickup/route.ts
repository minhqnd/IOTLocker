import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { locker_id, order_code, compartment } = body;

    if (!locker_id || !order_code || !compartment) {
      return NextResponse.json({ error: 'Missing required parameters' }, { status: 400 });
    }

    const cleanOrderCode = String(order_code).trim();

    // 1. Fetch order details from database
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

    // Protection check: COD order must be paid before pickup is logged
    if (order.is_cod && !order.paid) {
      return NextResponse.json({ error: 'Order is not paid yet' }, { status: 400 });
    }

    // 2. Update order status to 'picked'
    const { error: updateError } = await supabase
      .from('orders')
      .update({
        status: 'picked',
        updated_at: new Date().toISOString(),
      })
      .eq('id', order.id);

    if (updateError) {
      return NextResponse.json({ error: updateError.message }, { status: 500 });
    }

    // 3. Log event history
    await supabase.from('events').insert({
      type: 'pickup',
      locker_id,
      compartment,
      order_code: cleanOrderCode,
      payload: {
        previous_status: order.status,
        new_status: 'picked',
      },
    });

    return NextResponse.json({ ok: true });
  } catch (error: any) {
    return NextResponse.json({ error: error.message || 'Internal Server Error' }, { status: 500 });
  }
}
