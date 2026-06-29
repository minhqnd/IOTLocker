/**
 * Extracts a 6-digit numeric order code from the bank transfer transaction content.
 * Example: "CK MA DON 102938 CUA KHACH HANG" -> "102938"
 */
export function extractOrderCode(content: string): string | null {
  if (!content) return null;

  // Match exactly 6 digits with word boundaries to prevent matching part of longer numbers
  const match = content.match(/\b\d{6}\b/);
  return match ? match[0] : null;
}

