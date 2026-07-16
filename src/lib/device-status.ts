export const DEVICE_OFFLINE_AFTER_MS = 30000;

export function isDeviceOnline(lastSeen: string | null | undefined, now = Date.now()) {
  if (!lastSeen) return false;
  const seenAt = new Date(lastSeen).getTime();
  if (Number.isNaN(seenAt)) return false;
  return now - seenAt <= DEVICE_OFFLINE_AFTER_MS;
}
