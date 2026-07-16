import { supabase } from './supabase';
import { makePaymentId } from './sepay';
import { generateVietQRString } from './vietqr';

export const FREE_MINUTES = Number(process.env.FREE_MINUTES || 30);
export const OVERDUE_FEE = Number(process.env.OVERDUE_FEE || 5000);

export type LockerSession = {
  id: string;
  device_id: string;
  uid: string;
  locker_number: number;
  deposited_at: string;
  picked_up_at: string | null;
  is_active: boolean;
  payment_status: 'none' | 'pending' | 'paid' | 'waived';
  payment_id: string | null;
  fee_amount: number;
  paid_at: string | null;
  created_at: string;
  updated_at: string;
};

export function cleanText(value: unknown) {
  return String(value || '').trim();
}

export function cleanUid(value: unknown) {
  return cleanText(value).toUpperCase();
}

export function isOverdue(depositedAt: string) {
  const startedAt = new Date(depositedAt).getTime();
  return Date.now() - startedAt > FREE_MINUTES * 60 * 1000;
}

export async function logEvent(
  type: string,
  payload: {
    device_id?: string;
    uid?: string;
    locker_number?: number;
    session_id?: string;
    payload?: Record<string, unknown>;
  }
) {
  await supabase.from('events').insert({ type, ...payload });
}

export async function touchLocker(deviceId: string) {
  await supabase.from('lockers').upsert(
    {
      device_id: deviceId,
      last_seen: new Date().toISOString(),
      online: true,
    },
    { onConflict: 'device_id' }
  );
}

export async function findActiveSession(deviceId: string, uid: string) {
  return supabase
    .from('locker_sessions')
    .select('*')
    .eq('device_id', deviceId)
    .eq('uid', uid)
    .eq('is_active', true)
    .maybeSingle<LockerSession>();
}

export async function ensurePayment(session: LockerSession) {
  if (session.payment_id && session.payment_status === 'pending') {
    return session;
  }

  const paymentId = session.payment_id || makePaymentId();
  const { data, error } = await supabase
    .from('locker_sessions')
    .update({
      payment_id: paymentId,
      payment_status: 'pending',
      fee_amount: OVERDUE_FEE,
      updated_at: new Date().toISOString(),
    })
    .eq('id', session.id)
    .select()
    .single<LockerSession>();

  if (error) throw error;
  return data;
}

export function qrPayloadFor(session: LockerSession) {
  return generateVietQRString({
    amount: session.fee_amount || OVERDUE_FEE,
    orderCode: session.payment_id || '',
  });
}

export function sessionResponse(session: LockerSession, extra: Record<string, unknown> = {}) {
  return {
    sessionId: session.id,
    deviceId: session.device_id,
    uid: session.uid,
    locker: session.locker_number,
    depositedAt: session.deposited_at,
    paymentStatus: session.payment_status,
    fee: session.fee_amount,
    ...extra,
  };
}
