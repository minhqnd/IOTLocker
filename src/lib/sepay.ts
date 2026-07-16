import { randomUUID } from 'crypto';

export const PAYMENT_PREFIX = 'IOT';

export function makePaymentId() {
  const randomPart = randomUUID().replaceAll('-', '').slice(0, 8).toUpperCase();
  return `${PAYMENT_PREFIX}${randomPart}`;
}

/**
 * Sepay sends the bank transfer content back to us, so only accept our own
 * prefixed code. Plain 6 digits are too easy to match by accident.
 */
export function extractOrderCode(content: string): string | null {
  if (!content) return null;

  const match = content.toUpperCase().match(/\bIOT[A-Z0-9]{8}\b/);
  return match ? match[0] : null;
}
