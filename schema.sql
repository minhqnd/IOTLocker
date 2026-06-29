-- Create orders table
CREATE TABLE IF NOT EXISTS orders (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  order_code TEXT UNIQUE NOT NULL,       -- Used as VietQR memo / bank transfer description
  amount INTEGER NOT NULL,               -- Amount in VND
  is_cod BOOLEAN NOT NULL DEFAULT false, -- Cash On Delivery flag
  paid BOOLEAN NOT NULL DEFAULT false,   -- Payment status
  pin TEXT,                              -- 6-digit PIN code generated on deposit
  locker_id TEXT,                        -- Locker terminal identifier
  compartment TEXT,                      -- Compartment code (e.g., C1, C2)
  status TEXT NOT NULL DEFAULT 'created',-- Status flow: created | stored | awaiting_payment | paid | picked
  sepay_ref TEXT UNIQUE,                 -- SePay transaction reference ID (for idempotency)
  created_at TIMESTAMPTZ DEFAULT now(),
  updated_at TIMESTAMPTZ DEFAULT now()
);

-- Create events table
CREATE TABLE IF NOT EXISTS events (
  id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  type TEXT NOT NULL,                    -- Event types: deposit | pickup | heartbeat
  locker_id TEXT,
  compartment TEXT,
  order_code TEXT,
  payload JSONB,
  created_at TIMESTAMPTZ DEFAULT now()
);

-- Create lockers table
CREATE TABLE IF NOT EXISTS lockers (
  locker_id TEXT PRIMARY KEY,
  last_seen TIMESTAMPTZ DEFAULT now(),
  online BOOLEAN DEFAULT false
);
