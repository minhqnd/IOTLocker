import { createClient } from '@supabase/supabase-js';

const supabaseUrl =
  process.env.SUPABASE_SERVICE_ROLE_KEY_SUPABASE_URL ||
  process.env.NEXT_PUBLIC_SUPABASE_SERVICE_ROLE_KEY_SUPABASE_URL ||
  process.env.NEXT_PUBLIC_SUPABASE_URL;
const supabaseServiceKey =
  process.env.SUPABASE_SERVICE_ROLE_KEY_SUPABASE_SERVICE_ROLE_KEY ||
  process.env.SUPABASE_SERVICE_ROLE_KEY_SUPABASE_SECRET_KEY ||
  process.env.SUPABASE_SERVICE_ROLE_KEY;

if (!supabaseUrl || !supabaseServiceKey) {
  console.warn('Warning: Missing Supabase environment variables in .env.local');
}

// We initialize the client using the service role key to bypass RLS on backend API calls.
export const supabase = createClient(
  supabaseUrl || 'https://placeholder.supabase.co',
  supabaseServiceKey || 'placeholder-key',
  {
    auth: {
      persistSession: false,
      autoRefreshToken: false,
    },
  }
);
