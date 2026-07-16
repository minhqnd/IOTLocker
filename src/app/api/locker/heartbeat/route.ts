import { cleanText, touchLocker } from '@/lib/locker';

export const dynamic = 'force-dynamic';

export async function POST(request: Request) {
  try {
    const body = await request.json();
    const deviceId = cleanText(body.deviceId || body.device_id || 'locker-01');

    if (!deviceId) {
      return Response.json({ ok: false, error: 'Missing deviceId' }, { status: 400 });
    }

    await touchLocker(deviceId);
    return Response.json({ ok: true });
  } catch (error: unknown) {
    return Response.json({ ok: false, error: error instanceof Error ? error.message : 'Internal Server Error' }, { status: 500 });
  }
}
