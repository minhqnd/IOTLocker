import { supabase } from '@/lib/supabase';
import { isDeviceOnline } from '@/lib/device-status';

export const dynamic = 'force-dynamic';

export async function GET() {
  try {
    const [sessionsResult, lockersResult, eventsResult] = await Promise.all([
      supabase.from('locker_sessions').select('*').order('created_at', { ascending: false }).limit(100),
      supabase.from('lockers').select('*').order('last_seen', { ascending: false }),
      supabase.from('events').select('*').order('created_at', { ascending: false }).limit(100),
    ]);

    const error = sessionsResult.error || lockersResult.error || eventsResult.error;
    if (error) {
      return Response.json({ ok: false, error: error.message }, { status: 500 });
    }

    return Response.json({
      ok: true,
      sessions: sessionsResult.data || [],
      lockers: (lockersResult.data || []).map((locker) => ({
        ...locker,
        online: isDeviceOnline(locker.last_seen),
      })),
      events: eventsResult.data || [],
    });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
