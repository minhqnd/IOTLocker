'use client';

import { useEffect, useMemo, useState, type ReactNode } from 'react';

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
  device_id: string | null;
  uid: string | null;
  locker_number: number | null;
  payload: Record<string, unknown> | null;
  created_at: string;
};

type AdminData = {
  sessions: LockerSession[];
  lockers: Locker[];
  events: EventLog[];
};

export default function AdminPage() {
  const [data, setData] = useState<AdminData>({ sessions: [], lockers: [], events: [] });
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');

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

  useEffect(() => {
    const startTimer = setTimeout(() => {
      void loadData();
    }, 0);
    const timer = setInterval(() => {
      void loadData();
    }, 5000);
    return () => {
      clearTimeout(startTimer);
      clearInterval(timer);
    };
  }, []);

  const activeSessions = useMemo(() => data.sessions.filter((session) => session.is_active), [data.sessions]);
  const paidCount = activeSessions.filter((session) => session.payment_status === 'paid').length;

  return (
    <main className="min-h-screen bg-zinc-50 text-zinc-950">
      <div className="mx-auto max-w-7xl px-5 py-6">
        <header className="mb-6 flex flex-col gap-3 border-b border-zinc-200 pb-5 sm:flex-row sm:items-end sm:justify-between">
          <div>
            <p className="text-xs font-semibold uppercase tracking-wide text-zinc-500">IOT102 Locker v2</p>
            <h1 className="mt-1 text-2xl font-semibold">Dashboard tủ RFID</h1>
          </div>
          <button
            onClick={loadData}
            className="h-9 rounded-md border border-zinc-300 bg-white px-3 text-sm font-medium hover:bg-zinc-100"
          >
            Làm mới
          </button>
        </header>

        {error && (
          <div className="mb-5 rounded-md border border-red-200 bg-red-50 px-4 py-3 text-sm text-red-700">
            {error}
          </div>
        )}

        <section className="grid gap-3 sm:grid-cols-3">
          <Metric label="Phiên đang dùng" value={String(activeSessions.length)} />
          <Metric label="Đã thanh toán" value={String(paidCount)} />
          <Metric label="Thiết bị" value={String(data.lockers.length)} />
        </section>

        <section className="mt-6 grid gap-6 lg:grid-cols-[1.5fr_1fr]">
          <Panel title="Phiên gửi đồ">
            {loading ? (
              <Empty>Đang tải...</Empty>
            ) : data.sessions.length === 0 ? (
              <Empty>Chưa có phiên gửi đồ.</Empty>
            ) : (
              <div className="overflow-x-auto">
                <table className="w-full min-w-[720px] text-left text-sm">
                  <thead className="border-b border-zinc-200 text-xs uppercase text-zinc-500">
                    <tr>
                      <th className="py-2 pr-3">UID</th>
                      <th className="py-2 pr-3">Hộc</th>
                      <th className="py-2 pr-3">Thiết bị</th>
                      <th className="py-2 pr-3">Trạng thái</th>
                      <th className="py-2 pr-3">Thanh toán</th>
                      <th className="py-2 pr-3">Thời gian gửi</th>
                    </tr>
                  </thead>
                  <tbody>
                    {data.sessions.map((session) => (
                      <tr key={session.id} className="border-b border-zinc-100">
                        <td className="py-2 pr-3 font-mono">{session.uid}</td>
                        <td className="py-2 pr-3">Hộc {session.locker_number}</td>
                        <td className="py-2 pr-3">{session.device_id}</td>
                        <td className="py-2 pr-3">
                          <Status active={session.is_active} />
                        </td>
                        <td className="py-2 pr-3">
                          <Payment status={session.payment_status} paymentId={session.payment_id} fee={session.fee_amount} />
                        </td>
                        <td className="py-2 pr-3 text-zinc-600">{formatTime(session.deposited_at)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </Panel>

          <div className="space-y-6">
            <Panel title="Tủ locker">
              {data.lockers.length === 0 ? (
                <Empty>Chưa có heartbeat.</Empty>
              ) : (
                <div className="space-y-3">
                  {data.lockers.map((locker) => {
                    return (
                      <div key={locker.device_id} className="rounded-md border border-zinc-200 bg-white p-3">
                        <div className="flex items-center justify-between">
                          <span className="font-medium">{locker.device_id}</span>
                          <span className={locker.online ? 'text-sm text-emerald-700' : 'text-sm text-zinc-500'}>
                            {locker.online ? 'Online' : 'Offline'}
                          </span>
                        </div>
                        <p className="mt-1 text-xs text-zinc-500">{formatTime(locker.last_seen)}</p>
                      </div>
                    );
                  })}
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
          </div>
        </section>
      </div>
    </main>
  );
}

function Metric({ label, value }: { label: string; value: string }) {
  return (
    <div className="rounded-md border border-zinc-200 bg-white p-4">
      <p className="text-sm text-zinc-500">{label}</p>
      <p className="mt-2 text-3xl font-semibold">{value}</p>
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

function Payment({ status, paymentId, fee }: { status: string; paymentId: string | null; fee: number }) {
  return (
    <div>
      <span className="rounded bg-zinc-100 px-2 py-1 text-xs text-zinc-700">{status}</span>
      {paymentId && <p className="mt-1 font-mono text-xs text-zinc-500">{paymentId} · {fee.toLocaleString('vi-VN')}đ</p>}
    </div>
  );
}

function formatTime(value: string) {
  return new Date(value).toLocaleString('vi-VN', {
    hour: '2-digit',
    minute: '2-digit',
    day: '2-digit',
    month: '2-digit',
  });
}
