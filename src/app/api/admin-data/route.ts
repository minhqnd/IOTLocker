import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function GET() {
  try {
    // 1. Fetch all orders
    const { data: orders, error: ordersError } = await supabase
      .from('orders')
      .select('*')
      .order('created_at', { ascending: false });

    // 2. Fetch all lockers
    const { data: lockers, error: lockersError } = await supabase
      .from('lockers')
      .select('*')
      .order('last_seen', { ascending: false });

    // 3. Fetch latest 50 events
    const { data: events, error: eventsError } = await supabase
      .from('events')
      .select('*')
      .order('created_at', { ascending: false })
      .limit(50);

    if (ordersError || lockersError || eventsError) {
      console.error('Database fetch error in admin data aggregator:', {
        ordersError,
        lockersError,
        eventsError,
      });
      return NextResponse.json({
        error: 'Failed to fetch admin dashboard statistics'
      }, { status: 500 });
    }

    return NextResponse.json({
      ok: true,
      orders: orders || [],
      lockers: lockers || [],
      events: events || [],
    });
  } catch (error: any) {
    return NextResponse.json({ error: error.message || 'Internal Server Error' }, { status: 500 });
  }
}
