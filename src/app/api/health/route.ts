export const dynamic = 'force-dynamic';

export async function GET() {
  return Response.json({ ok: true, service: 'iot-locker-v2' });
}
