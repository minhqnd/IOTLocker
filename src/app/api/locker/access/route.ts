import {
  ensurePayment,
  findActiveSession,
  isOverdue,
  qrPayloadFor,
  sessionResponse,
  touchLocker,
} from '@/lib/locker';
import { supabase } from '@/lib/supabase';

export const dynamic = 'force-dynamic';

export async function GET(request: Request) {
  try {
    const { searchParams } = new URL(request.url);
    const deviceId = (searchParams.get('deviceId') || searchParams.get('device_id') || 'locker-01').trim();
    const uid = (searchParams.get('uid') || '').trim().toUpperCase();
    const mode = (searchParams.get('mode') || '').trim().toUpperCase();

    if (!deviceId || !uid) {
      return Response.json({ ok: false, error: 'Missing deviceId or uid' }, { status: 400 });
    }

    await touchLocker(deviceId);

    const { data: session, error } = await findActiveSession(deviceId, uid);
    if (error) {
      return Response.json({ ok: false, error: error.message }, { status: 500 });
    }

    if (!session) {
      return Response.json({ ok: true, found: false, allowOpen: false, code: 'NOT_FOUND' });
    }

    const overdue = isOverdue(session.deposited_at);
    const paid = session.payment_status === 'paid' || session.payment_status === 'waived';
    if (!overdue || paid) {
      await supabase
        .from('events')
        .insert({
          type: 'access_allow',
          device_id: deviceId,
          uid,
          locker_number: session.locker_number,
          session_id: session.id,
          payload: { mode, overdue, paid },
        });

      return Response.json(
        sessionResponse(session, {
          ok: true,
          found: true,
          overdue,
          paid,
          allowOpen: true,
        })
      );
    }

    const payableSession = await ensurePayment(session);
    return Response.json(
      sessionResponse(payableSession, {
        ok: true,
        found: true,
        overdue: true,
        paid: false,
        allowOpen: false,
        paymentId: payableSession.payment_id,
        qrPayload: qrPayloadFor(payableSession),
      })
    );
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
