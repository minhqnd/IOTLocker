import { cleanText, cleanUid, logEvent } from '@/lib/locker';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const uid = cleanUid(body.uid);
    const active = body.active !== false;
    const note = cleanText(body.note);

    if (!/^[0-9A-F]{6,14}$/.test(uid)) {
      return Response.json({ ok: false, error: 'UID must contain 6-14 hex characters' }, { status: 400 });
    }

    const { error } = await supabase
      .from('parking_cards')
      .upsert({ uid, active, note: note || null }, { onConflict: 'uid' });

    if (error) return Response.json({ ok: false, error: error.message }, { status: 500 });

    await logEvent(active ? 'parking_card_added' : 'parking_card_disabled', { uid, payload: { note } });
    return Response.json({ ok: true, uid, active });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
