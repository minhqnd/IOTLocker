import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const orderCode = searchParams.get('order_code');

    if (!orderCode) {
      return NextResponse.json({ error: 'Missing order_code query parameter' }, { status: 400 });
    }

    const cleanOrderCode = String(orderCode).trim();

    // Retrieve order status and pin details
    const { data: order, error: fetchError } = await supabase
      .from('orders')
      .select('paid, status, pin')
      .eq('order_code', cleanOrderCode)
      .maybeSingle();

    if (fetchError) {
      return NextResponse.json({ error: fetchError.message }, { status: 500 });
    }

    if (!order) {
      return NextResponse.json({ error: `Order with code ${cleanOrderCode} not found` }, { status: 404 });
    }

    const response: { paid: boolean; status: string; pin?: string } = {
      paid: order.paid,
      status: order.status,
    };

    // The customer payment page (web) only reveals the PIN after payment is completed
    if (order.paid && order.pin) {
      response.pin = order.pin;
    }

    return NextResponse.json(response);
  } catch (error: any) {
    return NextResponse.json({ error: error.message || 'Internal Server Error' }, { status: 500 });
  }
}
