create table if not exists locker_sessions (
  id uuid primary key default gen_random_uuid(),
  device_id text not null,
  uid text not null,
  locker_number integer not null,
  deposited_at timestamptz not null default now(),
  picked_up_at timestamptz,
  is_active boolean not null default true,
  payment_status text not null default 'none',
  payment_id text unique,
  fee_amount integer not null default 0,
  paid_at timestamptz,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

create unique index if not exists locker_sessions_active_uid_idx
  on locker_sessions(device_id, uid)
  where is_active = true;

create unique index if not exists locker_sessions_active_locker_idx
  on locker_sessions(device_id, locker_number)
  where is_active = true;

create table if not exists lockers (
  device_id text primary key,
  last_seen timestamptz not null default now(),
  online boolean not null default true
);

create table if not exists events (
  id uuid primary key default gen_random_uuid(),
  type text not null,
  device_id text,
  uid text,
  locker_number integer,
  session_id uuid,
  payload jsonb,
  created_at timestamptz not null default now()
);

create table if not exists parking_cards (
  uid text primary key,
  active boolean not null default true,
  note text,
  created_at timestamptz not null default now()
);

grant usage on schema public to anon;
grant select on table locker_sessions, lockers, events to anon;

do $$
begin
  alter publication supabase_realtime add table locker_sessions;
exception
  when duplicate_object then null;
end $$;

do $$
begin
  alter publication supabase_realtime add table lockers;
exception
  when duplicate_object then null;
end $$;

do $$
begin
  alter publication supabase_realtime add table events;
exception
  when duplicate_object then null;
end $$;
