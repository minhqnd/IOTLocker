import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';
import { generateVietQRString } from '@/lib/vietqr';

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const orderCode = searchParams.get('order_code');

    if (!orderCode) {
      return NextResponse.json({ error: 'Missing order_code query parameter' }, { status: 400 });
    }

    const cleanOrderCode = String(orderCode).trim();

    // 1. Retrieve the order from database
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

    // Double check that it is actually a COD order
    if (!order.is_cod) {
      return NextResponse.json({ error: 'Order is not a COD order (prepaid orders do not need VietQR)' }, { status: 400 });
    }

    // 2. Generate the EMVCo VietQR string
    const qrString = generateVietQRString({
      amount: order.amount,
      orderCode: cleanOrderCode,
    });

    return NextResponse.json({
      qr_string: qrString,
      amount: order.amount,
    });
  } catch (error: any) {
    return NextResponse.json({ error: error.message || 'Internal Server Error' }, { status: 500 });
  }
}
