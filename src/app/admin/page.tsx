'use client';

import { useEffect, useMemo, useRef, useState, type ReactNode } from 'react';
import { createBrowserSupabase } from '@/lib/supabase-browser';

const DEVICE_ID = 'locker-01';
const LOCKER_COUNT = 4;

type LockerSession = {
  id: string;
  device_id: string;
  uid: string;
  locker_number: number;
  deposited_at: string;
  picked_up_at: string | null;
  is_active: boolean;
  payment_status: string;
  payment_id: string | null;
  fee_amount: number;
};

type Locker = {
  device_id: string;
  last_seen: string;
  online: boolean;
};

type EventLog = {
  id: string;
  type: string;
  uid: string | null;
  locker_number: number | null;
  created_at: string;
};

type AdminData = {
  sessions: LockerSession[];
  lockers: Locker[];
  events: EventLog[];
};

type LockerSlot = {
  number: number;
  session?: LockerSession;
};

export default function AdminPage() {
  const [data, setData] = useState<AdminData>({ sessions: [], lockers: [], events: [] });
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState('');
  const [error, setError] = useState('');
  const [now, setNow] = useState(Date.now());
  const [realtimeStatus, setRealtimeStatus] = useState('Đang nối');
  const [lastRealtimeAt, setLastRealtimeAt] = useState<number | null>(null);
  const realtimeReloadTimer = useRef<number | null>(null);

  async function loadData() {
    try {
      const response = await fetch('/api/admin-data', { cache: 'no-store' });
      const json = await response.json();
      if (!response.ok || !json.ok) throw new Error(json.error || 'Cannot load dashboard');
      setData({ sessions: json.sessions, lockers: json.lockers, events: json.events });
      setError('');
    } catch (err: unknown) {
      setError(err instanceof Error ? err.message : 'Cannot load dashboard');
    } finally {
      setLoading(false);
    }
  }

  async function runAction(locker: number, action: string) {
    const key = `${locker}:${action}`;
    setBusy(key);
    try {
      const response = await fetch('/api/admin-action', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ deviceId: DEVICE_ID, locker, action }),
      });
      const json = await response.json();
      if (!response.ok || !json.ok) throw new Error(json.error || 'Action failed');
      await loadData();
    } catch (err: unknown) {
      setError(err instanceof Error ? err.message : 'Action failed');
    } finally {
      setBusy('');
    }
  }

  useEffect(() => {
    void loadData();
    const clockTimer = setInterval(() => setNow(Date.now()), 1000);
    const supabase = createBrowserSupabase();

    if (!supabase) {
      setRealtimeStatus('Thiếu env');
      setError('Realtime chưa có public Supabase env, dashboard chỉ tải snapshot ban đầu.');
      return () => clearInterval(clockTimer);
    }

    function refreshFromRealtime() {
      setLastRealtimeAt(Date.now());
      if (realtimeReloadTimer.current) window.clearTimeout(realtimeReloadTimer.current);
      realtimeReloadTimer.current = window.setTimeout(() => void loadData(), 150);
    }

    function refreshWhenFocused() {
      if (document.visibilityState === 'visible') void loadData();
    }

    const channel = supabase
      .channel('admin-dashboard')
      .on('postgres_changes', { event: '*', schema: 'public', table: 'locker_sessions' }, refreshFromRealtime)
      .on('postgres_changes', { event: '*', schema: 'public', table: 'lockers' }, refreshFromRealtime)
      .on('postgres_changes', { event: '*', schema: 'public', table: 'events' }, refreshFromRealtime)
      .subscribe((status) => {
        if (status === 'SUBSCRIBED') setRealtimeStatus('Realtime OK');
        if (status === 'CHANNEL_ERROR') setError('Realtime chưa kết nối được, bấm Làm mới nếu cần.');
        if (status === 'CHANNEL_ERROR') setRealtimeStatus('Realtime lỗi');
        if (status === 'TIMED_OUT') setRealtimeStatus('Realtime timeout');
      });

    window.addEventListener('focus', loadData);
    document.addEventListener('visibilitychange', refreshWhenFocused);

    return () => {
      clearInterval(clockTimer);
      if (realtimeReloadTimer.current) window.clearTimeout(realtimeReloadTimer.current);
      window.removeEventListener('focus', loadData);
      document.removeEventListener('visibilitychange', refreshWhenFocused);
      void supabase.removeChannel(channel);
    };
  }, []);

  const activeSessions = useMemo(() => data.sessions.filter((session) => session.is_active), [data.sessions]);
  const slots = useMemo(() => buildSlots(activeSessions), [activeSessions]);
  const paidCount = activeSessions.filter((session) => session.payment_status === 'paid').length;
  const overdueCount = activeSessions.filter((session) => isOverdue(session.deposited_at, now)).length;
  const currentLocker = data.lockers.find((locker) => locker.device_id === DEVICE_ID) || data.lockers[0];

  return (
    <main className="min-h-screen bg-zinc-50 text-zinc-950">
      <div className="mx-auto max-w-7xl px-5 py-6">
        <header className="mb-6 flex flex-col gap-3 border-b border-zinc-200 pb-5 sm:flex-row sm:items-end sm:justify-between">
          <div>
            <p className="text-xs font-semibold uppercase text-zinc-500">IOT102 Locker v2</p>
            <h1 className="mt-1 text-2xl font-semibold">Mô phỏng tủ RFID 4 hộc</h1>
          </div>
          <button
            onClick={loadData}
            className="h-9 rounded-md border border-zinc-300 bg-white px-3 text-sm font-medium hover:bg-zinc-100"
          >
            Làm mới
          </button>
          <p className="text-xs text-zinc-500">
            {realtimeStatus}
            {lastRealtimeAt ? ` · ${formatClock(lastRealtimeAt)}` : ''}
          </p>
        </header>

        {error && <div className="mb-5 rounded-md border border-red-200 bg-red-50 px-4 py-3 text-sm text-red-700">{error}</div>}

        <section className="grid gap-3 sm:grid-cols-4">
          <Metric label="Hộc đang dùng" value={`${activeSessions.length}/${LOCKER_COUNT}`} />
          <Metric label="Quá hạn" value={String(overdueCount)} />
          <Metric label="Đã thanh toán" value={String(paidCount)} />
          <Metric label="Thiết bị" value={currentLocker?.online ? 'Online' : 'Offline'} muted={!currentLocker?.online} />
        </section>

        <section className="mt-6 grid gap-4 lg:grid-cols-4">
          {slots.map((slot) => (
            <LockerCard
              key={slot.number}
              slot={slot}
              now={now}
              busy={busy}
              onAction={runAction}
            />
          ))}
        </section>

        <section className="mt-6 grid gap-6 lg:grid-cols-[1.3fr_0.7fr]">
          <Panel title="Phiên gửi đồ">
            {loading ? (
              <Empty>Đang tải...</Empty>
            ) : data.sessions.length === 0 ? (
              <Empty>Chưa có phiên gửi đồ.</Empty>
            ) : (
              <div className="overflow-x-auto">
                <table className="w-full min-w-[760px] text-left text-sm">
                  <thead className="border-b border-zinc-200 text-xs uppercase text-zinc-500">
                    <tr>
                      <th className="py-2 pr-3">UID</th>
                      <th className="py-2 pr-3">Hộc</th>
                      <th className="py-2 pr-3">Trạng thái</th>
                      <th className="py-2 pr-3">Thanh toán</th>
                      <th className="py-2 pr-3">Đang gửi</th>
                      <th className="py-2 pr-3">Bắt đầu</th>
                    </tr>
                  </thead>
                  <tbody>
                    {data.sessions.map((session) => (
                      <tr key={session.id} className="border-b border-zinc-100">
                        <td className="py-2 pr-3 font-mono">{session.uid}</td>
                        <td className="py-2 pr-3">Hộc {session.locker_number}</td>
                        <td className="py-2 pr-3"><Status active={session.is_active} /></td>
                        <td className="py-2 pr-3"><Payment session={session} now={now} /></td>
                        <td className="py-2 pr-3 font-mono text-zinc-700">{session.is_active ? elapsed(session.deposited_at, now) : '-'}</td>
                        <td className="py-2 pr-3 text-zinc-600">{formatTime(session.deposited_at)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </Panel>

          <Panel title="Event mới nhất">
            {data.events.length === 0 ? (
              <Empty>Chưa có event.</Empty>
            ) : (
              <div className="space-y-2">
                {data.events.slice(0, 12).map((event) => (
                  <div key={event.id} className="rounded-md border border-zinc-200 bg-white p-3 text-sm">
                    <div className="flex items-center justify-between gap-3">
                      <span className="font-medium">{event.type}</span>
                      <span className="text-xs text-zinc-500">{formatTime(event.created_at)}</span>
                    </div>
                    <p className="mt-1 font-mono text-xs text-zinc-600">
                      {event.uid || '-'} {event.locker_number ? `| Hộc ${event.locker_number}` : ''}
                    </p>
                  </div>
                ))}
              </div>
            )}
          </Panel>
        </section>
      </div>
    </main>
  );
}

function LockerCard({ slot, now, busy, onAction }: { slot: LockerSlot; now: number; busy: string; onAction: (locker: number, action: string) => void }) {
  const session = slot.session;
  const occupied = Boolean(session);
  const overdue = session ? isOverdue(session.deposited_at, now) : false;

  return (
    <article className="rounded-md border border-zinc-200 bg-white p-4">
      <div className="flex items-start justify-between gap-3">
        <div>
          <p className="text-xs font-semibold uppercase text-zinc-500">Hộc {String(slot.number).padStart(2, '0')}</p>
          <h2 className="mt-1 text-lg font-semibold">{occupied ? 'Đang gửi' : 'Trống'}</h2>
        </div>
        <span className={occupied ? 'rounded bg-amber-50 px-2 py-1 text-xs text-amber-700' : 'rounded bg-emerald-50 px-2 py-1 text-xs text-emerald-700'}>
          {occupied ? (overdue ? 'Quá hạn' : 'Trong hạn') : 'Sẵn sàng'}
        </span>
      </div>

      {session ? (
        <div className="mt-4 space-y-3 text-sm">
          <p className="font-mono text-zinc-700">{session.uid}</p>
          <div>
            <p className="text-xs text-zinc-500">Đang gửi</p>
            <p className="font-mono text-2xl font-semibold">{elapsed(session.deposited_at, now)}</p>
          </div>
          <Payment session={session} now={now} />
        </div>
      ) : (
        <Empty>Chưa có đồ trong hộc này.</Empty>
      )}

      <div className="mt-4 grid grid-cols-2 gap-2">
        {!session ? (
          <ActionButton busy={busy === `${slot.number}:deposit`} onClick={() => onAction(slot.number, 'deposit')}>Tạo gửi thử</ActionButton>
        ) : (
          <>
            <ActionButton busy={busy === `${slot.number}:overdue`} onClick={() => onAction(slot.number, 'overdue')}>Quá hạn</ActionButton>
            <ActionButton busy={busy === `${slot.number}:pending`} onClick={() => onAction(slot.number, 'pending')}>Chờ TT</ActionButton>
            <ActionButton busy={busy === `${slot.number}:paid`} onClick={() => onAction(slot.number, 'paid')}>Đã TT</ActionButton>
            <ActionButton busy={busy === `${slot.number}:pickup`} onClick={() => onAction(slot.number, 'pickup')}>Lấy đồ</ActionButton>
          </>
        )}
      </div>
    </article>
  );
}

function ActionButton({ children, busy, onClick }: { children: ReactNode; busy: boolean; onClick: () => void }) {
  return (
    <button
      disabled={busy}
      onClick={onClick}
      className="h-9 rounded-md border border-zinc-300 bg-white px-2 text-sm font-medium hover:bg-zinc-100 disabled:cursor-wait disabled:opacity-60"
    >
      {busy ? 'Đang...' : children}
    </button>
  );
}

function Metric({ label, value, muted = false }: { label: string; value: string; muted?: boolean }) {
  return (
    <div className="rounded-md border border-zinc-200 bg-white p-4">
      <p className="text-sm text-zinc-500">{label}</p>
      <p className={`mt-2 text-3xl font-semibold ${muted ? 'text-zinc-500' : ''}`}>{value}</p>
    </div>
  );
}

function Panel({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="rounded-md border border-zinc-200 bg-white p-4">
      <h2 className="mb-4 text-base font-semibold">{title}</h2>
      {children}
    </section>
  );
}

function Empty({ children }: { children: ReactNode }) {
  return <div className="rounded-md border border-dashed border-zinc-200 p-5 text-center text-sm text-zinc-500">{children}</div>;
}

function Status({ active }: { active: boolean }) {
  return (
    <span className={active ? 'rounded bg-emerald-50 px-2 py-1 text-xs text-emerald-700' : 'rounded bg-zinc-100 px-2 py-1 text-xs text-zinc-600'}>
      {active ? 'Đang dùng' : 'Đã lấy'}
    </span>
  );
}

function Payment({ session, now }: { session: LockerSession; now: number }) {
  const overdue = session.is_active && isOverdue(session.deposited_at, now);
  const color = paymentColor(session.payment_status, overdue);
  return (
    <div>
      <span className={`rounded px-2 py-1 text-xs ${color}`}>{paymentLabel(session.payment_status, overdue)}</span>
      {session.payment_id && <p className="mt-1 font-mono text-xs text-zinc-500">{session.payment_id} · {session.fee_amount.toLocaleString('vi-VN')}đ</p>}
    </div>
  );
}

function buildSlots(sessions: LockerSession[]) {
  return Array.from({ length: LOCKER_COUNT }, (_, index) => {
    const number = index + 1;
    return {
      number,
      session: sessions.find((session) => session.locker_number === number),
    };
  });
}

function paymentColor(status: string, overdue: boolean) {
  if (status === 'paid') return 'bg-emerald-50 text-emerald-700';
  if (status === 'pending' || overdue) return 'bg-amber-50 text-amber-700';
  return 'bg-zinc-100 text-zinc-700';
}

function paymentLabel(status: string, overdue: boolean) {
  if (status === 'paid') return 'Đã thanh toán';
  if (status === 'pending') return 'Chờ thanh toán';
  if (status === 'waived') return 'Bỏ qua phí';
  if (overdue) return 'Cần thanh toán';
  return 'Chưa cần TT';
}

function isOverdue(value: string, now: number) {
  return now - new Date(value).getTime() > 30 * 60 * 1000;
}

function elapsed(value: string, now: number) {
  const totalSeconds = Math.max(0, Math.floor((now - new Date(value).getTime()) / 1000));
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  if (hours > 0) return `${pad(hours)}:${pad(minutes)}:${pad(seconds)}`;
  return `${pad(minutes)}:${pad(seconds)}`;
}

function pad(value: number) {
  return String(value).padStart(2, '0');
}

function formatTime(value: string) {
  return new Date(value).toLocaleString('vi-VN', {
    hour: '2-digit',
    minute: '2-digit',
    day: '2-digit',
    month: '2-digit',
  });
}

function formatClock(value: number) {
  return new Date(value).toLocaleTimeString('vi-VN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}
