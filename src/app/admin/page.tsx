'use client';

import React, { useState, useEffect } from 'react';

interface Order {
  id: string;
  order_code: string;
  amount: number;
  is_cod: boolean;
  paid: boolean;
  pin: string | null;
  locker_id: string | null;
  compartment: string | null;
  status: string;
  sepay_ref: string | null;
  created_at: string;
  updated_at: string;
}

interface Locker {
  locker_id: string;
  last_seen: string;
  online: boolean;
}

interface EventLog {
  id: string;
  type: string;
  locker_id: string | null;
  compartment: string | null;
  order_code: string | null;
  payload: any;
  created_at: string;
}

export default function AdminDashboard() {
  const [orders, setOrders] = useState<Order[]>([]);
  const [lockers, setLockers] = useState<Locker[]>([]);
  const [events, setEvents] = useState<EventLog[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  // Form state
  const [formOrderCode, setFormOrderCode] = useState('');
  const [formAmount, setFormAmount] = useState('150000');
  const [formIsCod, setFormIsCod] = useState(true);
  const [formSubmitting, setFormSubmitting] = useState(false);
  const [formSuccessMessage, setFormSuccessMessage] = useState<string | null>(null);

  // Filter state
  const [statusFilter, setStatusFilter] = useState<string>('all');

  // Generate random 6-digit numeric code
  const generateRandomCode = () => {
    const code = Math.floor(100000 + Math.random() * 900000).toString();
    setFormOrderCode(code);
  };

  const fetchData = async () => {
    try {
      const response = await fetch('/api/admin-data');
      if (!response.ok) {
        throw new Error('Failed to fetch dashboard data');
      }
      const data = await response.json();
      if (data.ok) {
        setOrders(data.orders);
        setLockers(data.lockers);
        setEvents(data.events);
      }
    } catch (err: any) {
      console.error(err);
      setError(err.message || 'An error occurred while loading dashboard');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    generateRandomCode();
    fetchData();

    // Auto refresh every 5 seconds for live dashboard feeling
    const interval = setInterval(fetchData, 5000);
    return () => clearInterval(interval);
  }, []);

  const handleCreateOrder = async (e: React.FormEvent) => {
    e.preventDefault();
    if (!formOrderCode || !formAmount) return;

    setFormSubmitting(true);
    setFormSuccessMessage(null);
    setError(null);

    try {
      const response = await fetch('/api/orders', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          order_code: formOrderCode,
          amount: parseInt(formAmount, 10),
          is_cod: formIsCod,
        }),
      });

      const data = await response.json();
      if (!response.ok) {
        throw new Error(data.error || 'Failed to create order');
      }

      if (data.ok) {
        setFormSuccessMessage(`Đã tạo đơn ${formOrderCode} thành công!`);
        generateRandomCode(); // Generate new code for next order
        fetchData(); // Reload list
      }
    } catch (err: any) {
      setError(err.message || 'Error creating order');
    } finally {
      setFormSubmitting(false);
    }
  };

  // Helper to check if locker is active (within last 60 seconds)
  const isLockerActive = (lastSeenStr: string) => {
    const lastSeen = new Date(lastSeenStr).getTime();
    const now = new Date().getTime();
    return now - lastSeen < 60000; // 60 seconds timeout
  };

  const filteredOrders = orders.filter((order) => {
    if (statusFilter === 'all') return true;
    return order.status === statusFilter;
  });

  const getStatusBadgeClass = (status: string) => {
    switch (status) {
      case 'created':
        return 'bg-slate-100 text-slate-700 border-slate-200';
      case 'stored':
        return 'bg-blue-50 text-blue-700 border-blue-200';
      case 'awaiting_payment':
        return 'bg-amber-50 text-amber-700 border-amber-200';
      case 'paid':
        return 'bg-emerald-50 text-emerald-700 border-emerald-200';
      case 'picked':
        return 'bg-zinc-100 text-zinc-700 border-zinc-200';
      default:
        return 'bg-slate-100 text-slate-700 border-slate-200';
    }
  };

  const getStatusLabel = (status: string) => {
    switch (status) {
      case 'created':
        return 'Chờ gửi hàng';
      case 'stored':
        return 'Đã lưu (Trả trước)';
      case 'awaiting_payment':
        return 'Chờ thanh toán (COD)';
      case 'paid':
        return 'Đã thanh toán (Chờ lấy)';
      case 'picked':
        return 'Đã nhận hàng';
      default:
        return status;
    }
  };

  return (
    <div className="min-h-screen bg-slate-50 text-slate-800 font-sans p-6 sm:p-10">
      {/* Header */}
      <header className="max-w-7xl mx-auto mb-8 flex flex-col sm:flex-row items-start sm:items-center justify-between gap-4 border-b border-slate-200 pb-5">
        <div>
          <div className="flex items-center gap-2 mb-1">
            <span className="h-2 w-2 rounded-full bg-blue-600"></span>
            <span className="text-[10px] uppercase tracking-wider text-slate-500 font-bold font-mono">Quản lý tủ Locker</span>
          </div>
          <h1 className="text-2xl font-bold tracking-tight text-slate-900">
            Smart Delivery Drop-box Admin
          </h1>
        </div>
        <div className="flex items-center gap-3">
          <button
            onClick={fetchData}
            className="px-4 py-2 bg-white hover:bg-slate-50 border border-slate-250 text-slate-700 rounded-lg text-sm transition font-medium flex items-center gap-2 shadow-sm"
          >
            <svg xmlns="http://www.w3.org/2000/svg" className="h-4 w-4 text-slate-500" fill="none" viewBox="0 0 24 24" stroke="currentColor">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 1121.213 6H16" />
            </svg>
            Làm mới
          </button>
        </div>
      </header>

      {/* Main Grid */}
      <main className="max-w-7xl mx-auto grid grid-cols-1 lg:grid-cols-3 gap-8">
        
        {/* Left Column: Lockers and Order Creation */}
        <div className="space-y-8 lg:col-span-1">
          
          {/* Locker Status Card */}
          <section className="bg-white border border-slate-200 rounded-xl p-6 shadow-sm">
            <h2 className="text-base font-bold text-slate-900 mb-4 flex items-center gap-2 border-b border-slate-100 pb-2">
              <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 text-blue-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10" />
              </svg>
              Trạng thái Lockers
            </h2>
            
            {lockers.length === 0 ? (
              <div className="text-center py-6 text-slate-400 text-sm border border-dashed border-slate-200 rounded-lg">
                Chưa nhận tín hiệu từ tủ locker nào.
              </div>
            ) : (
              <div className="space-y-3">
                {lockers.map((locker) => {
                  const online = isLockerActive(locker.last_seen);
                  return (
                    <div
                      key={locker.locker_id}
                      className="flex items-center justify-between p-3.5 bg-slate-50 border border-slate-100 rounded-lg"
                    >
                      <div>
                        <div className="font-semibold text-slate-800 text-sm">{locker.locker_id}</div>
                        <div className="text-[11px] text-slate-500 mt-0.5 font-mono">
                          Cập nhật: {new Date(locker.last_seen).toLocaleTimeString()}
                        </div>
                      </div>
                      <span
                        className={`px-2.5 py-0.5 text-xs rounded-full border font-medium flex items-center gap-1.5 ${
                          online
                            ? 'bg-emerald-50 text-emerald-700 border-emerald-250'
                            : 'bg-rose-50 text-rose-700 border-rose-250'
                        }`}
                      >
                        <span className={`h-1.5 w-1.5 rounded-full ${online ? 'bg-emerald-500' : 'bg-rose-500'}`}></span>
                        {online ? 'Online' : 'Offline'}
                      </span>
                    </div>
                  );
                })}
              </div>
            )}
          </section>

          {/* Test Order Creation Form */}
          <section className="bg-white border border-slate-200 rounded-xl p-6 shadow-sm">
            <h2 className="text-base font-bold text-slate-900 mb-4 flex items-center gap-2 border-b border-slate-100 pb-2">
              <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 text-blue-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 13h6m-3-3v6m5 5H7a2 2 0 01-2-2V5a2 2 0 012-2h5.586a1 1 0 01.707.293l5.414 5.414a1 1 0 01.293.707V19a2 2 0 01-2 2z" />
              </svg>
              Tạo Đơn Hàng Test
            </h2>

            {formSuccessMessage && (
              <div className="mb-4 p-3 bg-emerald-50 border border-emerald-200 text-emerald-750 rounded-lg text-sm">
                {formSuccessMessage}
              </div>
            )}

            <form onSubmit={handleCreateOrder} className="space-y-4">
              <div>
                <label className="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5 font-mono">
                  Mã Đơn Hàng (6 chữ số)
                </label>
                <div className="flex gap-2">
                  <input
                    type="text"
                    maxLength={6}
                    value={formOrderCode}
                    onChange={(e) => setFormOrderCode(e.target.value.replace(/\D/g, ''))}
                    className="flex-1 bg-white border border-slate-300 focus:border-blue-500 rounded-lg px-3 py-2 text-sm text-slate-800 outline-none font-mono tracking-widest text-center transition"
                    placeholder="123456"
                    required
                  />
                  <button
                    type="button"
                    onClick={generateRandomCode}
                    className="px-3 bg-slate-50 hover:bg-slate-100 border border-slate-300 rounded-lg transition"
                    title="Tạo mã ngẫu nhiên"
                  >
                    🎲
                  </button>
                </div>
              </div>

              <div>
                <label className="block text-xs font-semibold text-slate-500 uppercase tracking-wider mb-1.5 font-mono">
                  Số Tiền Cần Thu (VND)
                </label>
                <input
                  type="number"
                  value={formAmount}
                  onChange={(e) => setFormAmount(e.target.value)}
                  className="w-full bg-white border border-slate-300 focus:border-blue-500 rounded-lg px-3 py-2 text-sm text-slate-800 outline-none font-mono transition"
                  placeholder="150000"
                  min="0"
                  required
                />
              </div>

              <div className="flex items-center justify-between p-3.5 bg-slate-50 border border-slate-200/60 rounded-lg">
                <span className="text-sm text-slate-650 font-medium">Thu hộ COD</span>
                <label className="relative inline-flex items-center cursor-pointer">
                  <input
                    type="checkbox"
                    checked={formIsCod}
                    onChange={(e) => setFormIsCod(e.target.checked)}
                    className="sr-only peer"
                  />
                  <div className="w-9 h-5 bg-slate-300 peer-focus:outline-none rounded-full peer peer-checked:after:translate-x-full peer-checked:after:border-white after:content-[''] after:absolute after:top-[2px] after:left-[2px] after:bg-white after:rounded-full after:h-4 after:w-4 after:transition-all peer-checked:bg-blue-600"></div>
                </label>
              </div>

              <button
                type="submit"
                disabled={formSubmitting}
                className="w-full py-2.5 bg-blue-600 hover:bg-blue-750 text-white rounded-lg text-sm font-semibold transition disabled:opacity-50 shadow-sm"
              >
                {formSubmitting ? 'Đang xử lý...' : 'Tạo Đơn Hàng'}
              </button>
            </form>
          </section>
        </div>

        {/* Right Columns: Orders list & Event Logs */}
        <div className="lg:col-span-2 space-y-8">
          
          {/* Order List Card */}
          <section className="bg-white border border-slate-200 rounded-xl p-6 shadow-sm">
            <div className="flex flex-col sm:flex-row justify-between items-start sm:items-center gap-4 mb-5 border-b border-slate-100 pb-3">
              <h2 className="text-base font-bold text-slate-900 flex items-center gap-2">
                <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 text-blue-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                  <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M12 8c-1.657 0-3 .895-3 2s1.343 2 3 2 3 .895 3 2-1.343 2-3 2m0-8c1.11 0 2.08.402 2.599 1M12 8V7m0 1v8m0 0v1m0-1c-1.11 0-2.08-.402-2.599-1M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                </svg>
                Danh sách đơn hàng
              </h2>
              
              {/* Order Status Filters */}
              <div className="flex flex-wrap gap-1 bg-slate-100 p-1 border border-slate-200 rounded-lg">
                {['all', 'created', 'stored', 'awaiting_payment', 'paid', 'picked'].map((tab) => (
                  <button
                    key={tab}
                    onClick={() => setStatusFilter(tab)}
                    className={`px-3 py-1 rounded text-xs transition capitalize ${
                      statusFilter === tab
                        ? 'bg-white text-slate-800 font-semibold shadow-sm border border-slate-200/50'
                        : 'text-slate-500 hover:text-slate-800'
                    }`}
                  >
                    {tab === 'all' ? 'Tất cả' : tab.replace('_', ' ')}
                  </button>
                ))}
              </div>
            </div>

            {loading && orders.length === 0 ? (
              <div className="text-center py-20 text-slate-400 text-sm animate-pulse">
                Đang tải dữ liệu đơn hàng...
              </div>
            ) : filteredOrders.length === 0 ? (
              <div className="text-center py-20 text-slate-450 text-sm border border-dashed border-slate-200 rounded-lg">
                Không tìm thấy đơn hàng tương ứng.
              </div>
            ) : (
              <div className="overflow-x-auto">
                <table className="w-full text-left border-collapse">
                  <thead>
                    <tr className="border-b border-slate-200 text-xs font-semibold text-slate-450 uppercase font-mono">
                      <th className="pb-3 pr-2">Mã đơn</th>
                      <th className="pb-3">Tiền thu</th>
                      <th className="pb-3">Loại</th>
                      <th className="pb-3">PIN</th>
                      <th className="pb-3">Hộc / Tủ</th>
                      <th className="pb-3 text-center">Trạng thái</th>
                      <th className="pb-3 text-right">Ngày tạo</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-100 text-sm text-slate-650">
                    {filteredOrders.map((order) => (
                      <tr key={order.id} className="hover:bg-slate-50/50 transition">
                        <td className="py-3 font-bold font-mono text-slate-900">
                          {order.order_code}
                        </td>
                        <td className="py-3 font-mono text-slate-600">
                          {order.amount.toLocaleString()}đ
                        </td>
                        <td className="py-3">
                          <span
                            className={`px-2 py-0.5 text-xs rounded font-medium ${
                              order.is_cod
                                ? 'bg-amber-100 text-amber-800'
                                : 'bg-slate-100 text-slate-800'
                            }`}
                          >
                            {order.is_cod ? 'COD' : 'Đã Trả'}
                          </span>
                        </td>
                        <td className="py-3 font-mono text-slate-800">
                          {order.pin ? (
                            <span className="bg-slate-100 px-2 py-0.5 border border-slate-200 rounded text-xs font-semibold">
                              {order.pin}
                            </span>
                          ) : (
                            <span className="text-slate-400">—</span>
                          )}
                        </td>
                        <td className="py-3 font-mono text-xs">
                          {order.locker_id ? (
                            <span className="text-slate-600">
                              {order.locker_id} <span className="text-slate-400">/</span> {order.compartment}
                            </span>
                          ) : (
                            <span className="text-slate-400">—</span>
                          )}
                        </td>
                        <td className="py-3 text-center">
                          <span
                            className={`inline-block px-2.5 py-0.5 text-xs font-semibold rounded-full border ${getStatusBadgeClass(
                              order.status
                            )}`}
                          >
                            {getStatusLabel(order.status)}
                          </span>
                        </td>
                        <td className="py-3 text-right text-xs text-slate-450 font-mono">
                          {new Date(order.created_at).toLocaleDateString()}{' '}
                          {new Date(order.created_at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' })}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </section>

          {/* System Events Log Terminal */}
          <section className="bg-white border border-slate-200 rounded-xl p-6 shadow-sm">
            <h2 className="text-base font-bold text-slate-900 mb-4 flex items-center gap-2 border-b border-slate-100 pb-2">
              <svg xmlns="http://www.w3.org/2000/svg" className="h-5 w-5 text-blue-600" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8 9l3 3-3 3m5 0h3M5 20h14a2 2 0 002-2V6a2 2 0 00-2-2H5a2 2 0 00-2 2v12a2 2 0 002 2z" />
              </svg>
              Nhật ký sự kiện thời gian thực
            </h2>

            <div className="bg-slate-50 rounded-xl p-4 h-60 overflow-y-auto font-mono text-[11px] border border-slate-200 text-slate-600 space-y-2">
              {events.length === 0 ? (
                <div className="text-slate-400 text-center py-20">Chưa có sự kiện nào được ghi nhận.</div>
              ) : (
                events.map((event) => {
                  let badgeColor = 'text-blue-600';
                  if (event.type === 'payment') badgeColor = 'text-emerald-650 font-bold';
                  if (event.type === 'pickup') badgeColor = 'text-teal-600 font-bold';
                  if (event.type === 'heartbeat') badgeColor = 'text-slate-400';

                  return (
                    <div key={event.id} className="leading-relaxed border-b border-slate-200/40 pb-1.5 last:border-0">
                      <span className="text-slate-400">[{new Date(event.created_at).toISOString()}]</span>{' '}
                      <span className={`${badgeColor}`}>{event.type.toUpperCase()}</span>:{' '}
                      {event.locker_id && (
                        <span>
                          Tủ <span className="text-slate-800 font-medium">{event.locker_id}</span>{' '}
                        </span>
                      )}
                      {event.compartment && (
                        <span>
                          hộc <span className="text-slate-800 font-medium">{event.compartment}</span>{' '}
                        </span>
                      )}
                      {event.order_code && (
                        <span>
                          đơn <span className="text-slate-900 font-bold font-mono">#{event.order_code}</span>{' '}
                        </span>
                      )}
                      {event.payload && Object.keys(event.payload).length > 0 && (
                        <span className="text-slate-450 block text-[10px] mt-0.5 ml-2 font-sans bg-white border border-slate-100 rounded px-1.5 py-0.5 w-fit">
                          Chi tiết: {JSON.stringify(event.payload)}
                        </span>
                      )}
                    </div>
                  );
                })
              )}
            </div>
          </section>
        </div>
      </main>
    </div>
  );
}
