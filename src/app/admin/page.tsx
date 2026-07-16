'use client';

import { useEffect, useMemo, useRef, useState, type ReactNode } from 'react';
import { isDeviceOnline } from '@/lib/device-status';
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
        if (status === 'SUBSCRIBED') setRealtimeStatus('Connected');
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
  const deviceOnline = isDeviceOnline(currentLocker?.last_seen, now);

  return (
    <main className="min-h-screen bg-[#f6f7f9] text-zinc-950">
      <div className="mx-auto max-w-7xl px-5 py-6">
        <header className="mb-6 border-b border-zinc-200 pb-5">
          <div className="flex flex-col gap-4 sm:flex-row sm:items-end sm:justify-between">
            <div>
              <p className="text-xs font-semibold uppercase text-zinc-500">IOT102 Locker</p>
              <h1 className="mt-1 text-3xl font-semibold tracking-normal">Quản lý Locker</h1>
            </div>
            <div className="flex flex-wrap items-center gap-3">
              <RealtimeBadge status={realtimeStatus} lastRealtimeAt={lastRealtimeAt} />
              <button
                onClick={loadData}
                className="h-10 rounded-md border border-zinc-300 bg-white px-4 text-sm font-medium shadow-sm hover:bg-zinc-100"
              >
                Làm mới
              </button>
            </div>
          </div>
        </header>

        {error && <div className="mb-5 rounded-md border border-red-200 bg-red-50 px-4 py-3 text-sm text-red-700">{error}</div>}

        <section className="grid gap-3 sm:grid-cols-4">
          <Metric label="Hộc đang dùng" value={`${activeSessions.length}/${LOCKER_COUNT}`} />
          <Metric label="Quá hạn" value={String(overdueCount)} />
          <Metric label="Đã thanh toán" value={String(paidCount)} />
          <Metric label="Thiết bị" value={deviceOnline ? 'Online' : 'Offline'} />
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

        <section className="mt-6 grid gap-6">
          <Panel title="Phiên gửi đồ">
            {loading ? (
              <Empty>Đang tải...</Empty>
            ) : data.sessions.length === 0 ? (
              <Empty>Chưa có phiên gửi đồ.</Empty>
            ) : (
              <div className="overflow-x-auto">
                <table className="w-full min-w-[760px] text-left text-sm">
                  <thead className="border-b border-zinc-200 bg-zinc-50 text-xs uppercase text-zinc-500">
                    <tr>
                      <th className="px-3 py-3">UID</th>
                      <th className="px-3 py-3">Hộc</th>
                      <th className="px-3 py-3">Trạng thái</th>
                      <th className="px-3 py-3">Thanh toán</th>
                      <th className="px-3 py-3">Đang gửi</th>
                      <th className="px-3 py-3">Bắt đầu</th>
                    </tr>
                  </thead>
                  <tbody>
                    {data.sessions.map((session) => (
                      <tr key={session.id} className="border-b border-zinc-100 last:border-0">
                        <td className="px-3 py-3 font-mono">{session.uid}</td>
                        <td className="px-3 py-3">Hộc {session.locker_number}</td>
                        <td className="px-3 py-3"><Status active={session.is_active} /></td>
                        <td className="px-3 py-3"><Payment session={session} now={now} /></td>
                        <td className="px-3 py-3 font-mono text-zinc-700">{session.is_active ? elapsed(session.deposited_at, now) : '-'}</td>
                        <td className="px-3 py-3 text-zinc-600">{formatTime(session.deposited_at)}</td>
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
              <div className="overflow-hidden rounded-md border border-zinc-200">
                {data.events.slice(0, 12).map((event, index) => (
                  <div
                    key={event.id}
                    className={`grid gap-3 border-b border-zinc-100 px-4 py-3 text-sm last:border-0 sm:grid-cols-[44px_1fr_auto] sm:items-center ${index === 0 ? 'bg-zinc-50' : 'bg-white'
                      }`}
                  >
                    <span className="font-mono text-xs font-medium text-zinc-400">#{index + 1}</span>
                    <div>
                      <div className="flex flex-wrap items-center gap-2">
                        <span className="font-medium text-zinc-950">{event.type}</span>
                        {/* {index === 0 ? <span className="rounded-md bg-zinc-900 px-2 py-0.5 text-xs font-medium text-white">Mới nhất</span> : null} */}
                      </div>
                      <p className="mt-1 font-mono text-xs text-zinc-600">
                        {event.uid || '-'} {event.locker_number ? `| Hộc ${event.locker_number}` : ''}
                      </p>
                    </div>
                    <time className="font-mono text-xs text-zinc-500 sm:text-right">{formatTime(event.created_at)}</time>
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
  const cardTone = overdue ? 'border-amber-200 bg-amber-50/30' : occupied ? 'border-zinc-300 bg-white' : 'border-zinc-200 bg-white';

  return (
    <article className={`flex min-h-[272px] flex-col rounded-md border p-4 shadow-sm ${cardTone}`}>
      <div className="flex items-start justify-between gap-3">
        <div>
          <p className="text-xs font-semibold uppercase text-zinc-500">Hộc {String(slot.number).padStart(2, '0')}</p>
          <h2 className="mt-1 text-2xl font-semibold">{occupied ? 'Đang gửi' : 'Trống'}</h2>
        </div>
        <LockerBadge occupied={occupied} overdue={overdue} />
      </div>

      {session ? (
        <div className="mt-5 flex-1 space-y-4 text-sm">
          <div className="rounded-md border border-zinc-200 bg-white/80 p-3">
            <p className="text-xs font-medium uppercase text-zinc-500">UID</p>
            <p className="mt-1 font-mono text-base text-zinc-800">{session.uid}</p>
          </div>
          <div>
            <p className="text-xs font-medium uppercase text-zinc-500">Đang gửi</p>
            <p className="font-mono text-4xl font-semibold leading-none">{elapsed(session.deposited_at, now)}</p>
            <div className="mt-3">
              <Payment session={session} now={now} />
            </div>
          </div>
        </div>
      ) : (
        <div className="mt-5 flex flex-1 items-center justify-center rounded-md border border-dashed border-zinc-200 bg-zinc-50 text-sm text-zinc-500">
          Chưa có đồ trong hộc này.
        </div>
      )}

      <div className="mt-4 grid grid-cols-2 gap-2">
        {!session ? (
          <ActionButton busy={busy === `${slot.number}:deposit`} onClick={() => onAction(slot.number, 'deposit')} wide>Tạo gửi thử</ActionButton>
        ) : (
          <>
            <ActionButton busy={busy === `${slot.number}:overdue`} onClick={() => onAction(slot.number, 'overdue')} tone="warning">Quá hạn</ActionButton>
            <ActionButton busy={busy === `${slot.number}:paid`} onClick={() => onAction(slot.number, 'paid')} tone="success">Đã TT</ActionButton>
            <ActionButton busy={busy === `${slot.number}:pickup`} onClick={() => onAction(slot.number, 'pickup')} wide>Lấy đồ</ActionButton>
          </>
        )}
      </div>
    </article>
  );
}

function LockerBadge({ occupied, overdue }: { occupied: boolean; overdue: boolean }) {
  if (!occupied) return <span className="rounded-md bg-emerald-50 px-2.5 py-1 text-xs font-medium text-emerald-700">Sẵn sàng</span>;
  if (overdue) return <span className="rounded-md bg-amber-100 px-2.5 py-1 text-xs font-medium text-amber-800">Quá hạn</span>;
  return <span className="rounded-md bg-blue-50 px-2.5 py-1 text-xs font-medium text-blue-700">Trong hạn</span>;
}

function ActionButton({ children, busy, onClick, tone = 'neutral', wide = false }: { children: ReactNode; busy: boolean; onClick: () => void; tone?: 'neutral' | 'warning' | 'success'; wide?: boolean }) {
  const toneClass = {
    neutral: 'border-zinc-300 bg-white text-zinc-900 hover:bg-zinc-100',
    warning: 'border-amber-200 bg-amber-50 text-amber-800 hover:bg-amber-100',
    success: 'border-emerald-200 bg-emerald-50 text-emerald-800 hover:bg-emerald-100',
  }[tone];

  return (
    <button
      disabled={busy}
      onClick={onClick}
      className={`h-10 rounded-md border px-2 text-sm font-medium shadow-sm disabled:cursor-wait disabled:opacity-60 ${toneClass} ${wide ? 'col-span-2' : ''}`}
    >
      {busy ? 'Đang...' : children}
    </button>
  );
}

function Metric({ label, value }: { label: string; value: string }) {
  return (
    <div className="rounded-md border border-zinc-200 bg-white p-4 shadow-sm">
      <p className="text-sm font-medium text-zinc-500">{label}</p>
      <p className="mt-2 text-4xl font-semibold leading-none">{value}</p>
    </div>
  );
}

function Panel({ title, children }: { title: string; children: ReactNode }) {
  return (
    <section className="rounded-md border border-zinc-200 bg-white p-5 shadow-sm">
      <h2 className="mb-4 text-xl font-semibold">{title}</h2>
      {children}
    </section>
  );
}

function Empty({ children }: { children: ReactNode }) {
  return <div className="rounded-md border border-dashed border-zinc-200 p-5 text-center text-sm text-zinc-500">{children}</div>;
}

function Status({ active }: { active: boolean }) {
  return (
    <span className={active ? 'rounded-md bg-emerald-50 px-2.5 py-1 text-xs font-medium text-emerald-700' : 'rounded-md bg-zinc-100 px-2.5 py-1 text-xs font-medium text-zinc-600'}>
      {active ? 'Đang dùng' : 'Đã lấy'}
    </span>
  );
}

function Payment({ session, now }: { session: LockerSession; now: number }) {
  const overdue = session.is_active && isOverdue(session.deposited_at, now);
  const color = paymentColor(session.payment_status, overdue);
  return (
    <div>
      <span className={`rounded-md px-2.5 py-1 text-xs font-medium ${color}`}>{paymentLabel(session.payment_status, overdue)}</span>
      {session.payment_id && <p className="mt-1 font-mono text-xs text-zinc-500">{session.payment_id} · {session.fee_amount.toLocaleString('vi-VN')}đ</p>}
    </div>
  );
}

function RealtimeBadge({ status, lastRealtimeAt }: { status: string; lastRealtimeAt: number | null }) {
  const ok = status === 'Connected';
  const bad = status.includes('lỗi') || status.includes('timeout') || status.includes('Thiếu');
  const color = ok ? 'bg-emerald-50 text-emerald-700 ring-emerald-200' : bad ? 'bg-red-50 text-red-700 ring-red-200' : 'bg-zinc-100 text-zinc-600 ring-zinc-200';

  return (
    <span className={`inline-flex h-10 items-center rounded-md px-3 text-sm font-medium ring-1 ${color}`}>
      {status}
      {lastRealtimeAt ? <span className="ml-2 font-mono text-xs opacity-70">{formatClock(lastRealtimeAt)}</span> : null}
    </span>
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
