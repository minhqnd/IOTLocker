'use client';

import React, { useState, useEffect, use } from 'react';

interface PaymentStatusResponse {
  paid: boolean;
  status: string;
  pin?: string;
}

interface QRResponse {
  qr_string: string;
  amount: number;
}

interface PageProps {
  params: Promise<{
    order_code: string;
  }>;
}

export default function CustomerPayPage({ params }: PageProps) {
  const { order_code } = use(params);

  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  
  const [paid, setPaid] = useState(false);
  const [orderStatus, setOrderStatus] = useState<string>('created');
  const [pin, setPin] = useState<string | null>(null);
  
  const [amount, setAmount] = useState<number | null>(null);
  const [qrString, setQrString] = useState<string | null>(null);

  // Fetch the order payment status and check if paid
  const checkPaymentStatus = async (initial = false) => {
    try {
      const res = await fetch(`/api/payment-status?order_code=${order_code}`);
      if (!res.ok) {
        if (res.status === 404) {
          throw new Error('Đơn hàng không tồn tại');
        }
        throw new Error('Không thể tải trạng thái đơn hàng');
      }

      const data: PaymentStatusResponse = await res.json();
      setPaid(data.paid);
      setOrderStatus(data.status);

      if (data.paid && data.pin) {
        setPin(data.pin);
      }

      // If it is not paid yet and this is the initial load, fetch the QR string
      if (initial && !data.paid) {
        await fetchQRDetails();
      }
    } catch (err: any) {
      console.error(err);
      setError(err.message || 'Lỗi kết nối máy chủ');
    } finally {
      if (initial) setLoading(false);
    }
  };

  const fetchQRDetails = async () => {
    try {
      const res = await fetch(`/api/qr?order_code=${order_code}`);
      if (!res.ok) {
        // If it fails, maybe order is not COD or has issues
        return;
      }
      const data: QRResponse = await res.json();
      setQrString(data.qr_string);
      setAmount(data.amount);
    } catch (err) {
      console.error('Error fetching QR:', err);
    }
  };

  useEffect(() => {
    checkPaymentStatus(true);

    // Set up polling interval every 3 seconds if not paid yet
    const interval = setInterval(() => {
      if (!paid) {
        checkPaymentStatus(false);
      }
    }, 3000);

    return () => clearInterval(interval);
  }, [order_code, paid]);

  if (loading) {
    return (
      <div className="min-h-screen bg-slate-50 flex flex-col items-center justify-center p-6 text-slate-500 font-sans">
        <div className="h-8 w-8 rounded-full border-2 border-slate-300 border-t-blue-600 animate-spin mb-4"></div>
        <p className="text-xs font-mono tracking-wider font-semibold">ĐANG TẢI THÔNG TIN THANH TOÁN...</p>
      </div>
    );
  }

  if (error) {
    return (
      <div className="min-h-screen bg-slate-50 flex flex-col items-center justify-center p-6 text-slate-800 font-sans">
        <div className="bg-white border border-slate-200 p-6 rounded-xl max-w-sm text-center shadow-sm">
          <div className="text-3xl mb-3">⚠️</div>
          <h2 className="text-base font-bold mb-1.5 text-slate-900">Đã xảy ra lỗi</h2>
          <p className="text-sm text-slate-555 mb-6">{error}</p>
          <a
            href="/admin"
            className="inline-block bg-slate-100 hover:bg-slate-200 border border-slate-300 text-slate-700 text-xs px-4 py-2 rounded-lg font-semibold transition"
          >
            Quay lại Admin Dashboard
          </a>
        </div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-slate-50 text-slate-750 font-sans flex flex-col justify-between py-10 px-4">
      <div className="max-w-md w-full mx-auto space-y-6">
        
        {/* Header App Identity */}
        <div className="text-center">
          <div className="inline-flex items-center gap-1.5 px-3 py-1 bg-white border border-slate-200 rounded-full mb-3 shadow-sm">
            <span className="h-1.5 w-1.5 rounded-full bg-blue-600"></span>
            <span className="text-[10px] tracking-wider text-slate-500 font-bold uppercase font-mono">Smart Delivery Box</span>
          </div>
          <h1 className="text-xl font-bold text-slate-900">Thanh Toán Đơn Hàng</h1>
          <p className="text-xs text-slate-500 mt-1 font-mono">Mã đơn: #{order_code}</p>
        </div>

        {/* Conditional state display */}
        {!paid ? (
          /* COD Payment pending screen */
          <div className="bg-white border border-slate-200 rounded-2xl p-6 space-y-6 shadow-sm">
            <div className="text-center space-y-1 pb-4 border-b border-slate-100">
              <span className="text-xs text-slate-450 uppercase tracking-wider font-semibold font-mono">Số tiền cần thanh toán</span>
              <div className="text-3xl font-extrabold text-slate-900 font-mono">
                {amount ? `${amount.toLocaleString()}đ` : '—'}
              </div>
            </div>

            {/* QR Code display */}
            {qrString ? (
              <div className="flex flex-col items-center space-y-4">
                <div className="bg-white p-2.5 border border-slate-200 rounded-xl">
                  {/* Generate QR image on-the-fly using secure free service for the EMVCo string */}
                  <img
                    src={`https://api.qrserver.com/v1/create-qr-code/?size=200x200&data=${encodeURIComponent(qrString)}`}
                    alt="VietQR Pay Code"
                    width={180}
                    height={180}
                    className="rounded-md"
                  />
                </div>
                <div className="text-center space-y-1">
                  <p className="text-xs font-semibold text-slate-800">Quét mã bằng ứng dụng Ngân hàng / Ví điện tử</p>
                  <p className="text-[10px] text-slate-500 max-w-[280px]">
                    Nội dung chuyển khoản cần chứa chính xác mã đơn: <span className="font-bold text-slate-800 font-mono">{order_code}</span>
                  </p>
                </div>
              </div>
            ) : (
              <div className="text-center py-10 text-slate-450 text-xs animate-pulse">
                Đang tạo mã VietQR...
              </div>
            )}

            {/* Wait status indicator */}
            <div className="flex items-center justify-center gap-2.5 bg-amber-50 border border-amber-100 rounded-xl p-3.5">
              <div className="h-1.5 w-1.5 rounded-full bg-amber-500 animate-pulse"></div>
              <span className="text-[11px] text-amber-800 font-semibold tracking-wide">
                Đang chờ hệ thống ghi nhận chuyển khoản...
              </span>
            </div>

            {/* Bank detail card fallback */}
            <div className="bg-slate-50 border border-slate-100 rounded-xl p-4 text-xs space-y-2">
              <div className="text-slate-500 uppercase font-bold tracking-wider text-[9px] mb-1 font-mono">Chuyển khoản thủ công:</div>
              <div className="flex justify-between border-b border-slate-200/40 pb-1.5"><span className="text-slate-500">Ngân hàng:</span><span className="text-slate-800 font-semibold">Vietcombank</span></div>
              <div className="flex justify-between border-b border-slate-200/40 pb-1.5"><span className="text-slate-500">Số tài khoản:</span><span className="text-slate-800 font-semibold font-mono">1017588888</span></div>
              <div className="flex justify-between border-b border-slate-200/40 pb-1.5"><span className="text-slate-500">Tên thụ hưởng:</span><span className="text-slate-800 font-semibold">NGUYEN VAN A</span></div>
              <div className="flex justify-between"><span className="text-slate-500">Nội dung CK:</span><span className="text-amber-700 font-bold font-mono">{order_code}</span></div>
            </div>
          </div>
        ) : (
          /* Payment successful & PIN reveal screen */
          <div className="space-y-6">
            {/* Success announcement card */}
            <div className="bg-emerald-50 border border-emerald-250 rounded-2xl p-6 text-center space-y-3 shadow-sm">
              <div className="h-12 w-12 bg-emerald-500 text-white rounded-full flex items-center justify-center mx-auto text-xl font-bold">
                ✓
              </div>
              <div className="space-y-1">
                <h2 className="text-base font-bold text-emerald-800">Thanh Toán Thành Công</h2>
                <p className="text-xs text-slate-500">Đơn hàng của bạn đã sẵn sàng tại tủ locker.</p>
              </div>
            </div>

            {/* Large PIN reveal card */}
            <div className="bg-white border border-slate-200 rounded-2xl p-6 text-center space-y-4 shadow-sm">
              <span className="text-xs text-slate-450 uppercase tracking-wider font-semibold font-mono">MÃ PIN NHẬN HÀNG</span>
              
              <div className="bg-slate-100 border border-slate-200 rounded-xl py-4 px-6 max-w-xs mx-auto">
                {pin ? (
                  <div className="text-3xl font-extrabold font-mono tracking-[0.3em] pl-[0.3em] text-slate-950">
                    {pin}
                  </div>
                ) : (
                  <div className="text-xs text-slate-400 font-mono animate-pulse">Đang lấy mã PIN...</div>
                )}
              </div>

              <p className="text-xs text-slate-500 leading-relaxed max-w-[280px] mx-auto">
                Nhập mã PIN gồm 6 chữ số này trên bàn phím tại tủ locker để nhận hàng.
              </p>
            </div>

            {/* Locker location/compartment details */}
            <div className="bg-white border border-slate-200 rounded-xl p-4 flex justify-between items-center text-sm shadow-sm">
              <div className="flex items-center gap-2.5">
                <span className="text-lg">🚪</span>
                <div>
                  <div className="text-slate-400 text-[10px] uppercase font-bold tracking-wider font-mono">Vị trí nhận hàng</div>
                  <div className="text-xs text-slate-700 font-semibold mt-0.5">Hộc tủ của bạn</div>
                </div>
              </div>
              <div className="font-mono text-sm font-bold bg-slate-100 text-slate-800 px-3 py-1 rounded-lg border border-slate-200">
                Hộc {orderStatus === 'picked' ? 'đã mở' : 'C1'}
              </div>
            </div>
          </div>
        )}
      </div>

      {/* Footer Info */}
      <footer className="text-center text-[10px] text-slate-400 mt-10">
        <p>© 2026 Smart Delivery Box Systems</p>
        <p className="mt-0.5">Thanh toán tự động thông qua cổng SePay</p>
      </footer>
    </div>
  );
}
