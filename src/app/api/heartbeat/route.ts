import { NextResponse } from 'next/server';
import { supabase } from '@/lib/supabase';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const { locker_id } = body;

    if (!locker_id) {
      return NextResponse.json({ error: 'Missing locker_id parameter' }, { status: 400 });
    }

    const cleanLockerId = String(locker_id).trim();

    // 1. Upsert the locker status
    const { error: upsertError } = await supabase
      .from('lockers')
      .upsert({
        locker_id: cleanLockerId,
        last_seen: new Date().toISOString(),
        online: true,
      }, { onConflict: 'locker_id' });

    if (upsertError) {
      return NextResponse.json({ error: upsertError.message }, { status: 500 });
    }

    // 2. Record the heartbeat event
    await supabase.from('events').insert({
      type: 'heartbeat',
      locker_id: cleanLockerId,
      payload: {},
    });

    return NextResponse.json({ ok: true });
  } catch (error: any) {
    return NextResponse.json({ error: error.message || 'Internal Server Error' }, { status: 500 });
  }
}
