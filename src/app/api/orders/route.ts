import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';

export async function GET() {
  try {
    const { data: orders, error } = await supabase
      .from('orders')
      .select('*')
      .order('created_at', { ascending: false });

    if (error) {
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    return NextResponse.json({ ok: true, orders });
  } catch (error: any) {
    return NextResponse.json({ error: error.message || 'Internal Server Error' }, { status: 500 });
  }
}

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { order_code, amount, is_cod } = body;

    if (!order_code || typeof amount !== 'number') {
      return NextResponse.json({ error: 'Missing or invalid order_code or amount' }, { status: 400 });
    }

    // Clean up order code (should be numeric 6 digits)
    const cleanOrderCode = String(order_code).trim();

    // Prepaid orders are marked paid = true immediately.
    // COD orders are marked paid = false.
    const paid = !is_cod;

    const { data: newOrder, error } = await supabase
      .from('orders')
      .insert({
        order_code: cleanOrderCode,
        amount,
        is_cod: !!is_cod,
        paid,
        status: 'created',
      })
      .select()
      .single();

    if (error) {
      return NextResponse.json({ error: error.message }, { status: 500 });
    }

    return NextResponse.json({ ok: true, order: newOrder });
  } catch (error: any) {
    return NextResponse.json({ error: error.message || 'Internal Server Error' }, { status: 500 });
  }
}
