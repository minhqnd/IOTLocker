/**
 * CRC16 checksum calculation for VietQR (EMVCo)
 */
function crc16(str: string): string {
  let crc = 0xffff;
  for (let i = 0; i < str.length; i++) {
    crc ^= str.charCodeAt(i) << 8;
    for (let j = 0; j < 8; j++) {
      if (crc & 0x8000) {
        crc = (crc << 1) ^ 0x1021;
      } else {
        crc = crc << 1;
      }
    }
  }
  crc = crc & 0xffff;
  return crc.toString(16).padStart(4, "0").toUpperCase();
}

/**
 * Pad a number to 2 digits with leading zeros
 */
const pad2zero = (num: number): string => num.toString().padStart(2, "0");

interface GenerateQRParams {
  amount: number;
  orderCode: string;
}

/**
 * Generates an EMVCo-compliant VietQR content string.
 * This string can be sent to ESP32 to display as a QR code on its OLED screen,
 * or rendered on the customer payment page.
 */
export function generateVietQRString({ amount, orderCode }: GenerateQRParams): string {
  const bankBin = process.env.BANK_BIN || '970436'; // Fallback to VCB BIN if not set
  const bankAccount = process.env.BANK_ACCOUNT || '1017588888'; // Fallback account

  const cleanBankBin = bankBin.trim();
  const cleanBankAccount = bankAccount.trim();
  const cleanOrderCode = orderCode.trim();

  // Construct VietQR (EMVCo) format
  let v = "000201010211" +
      "38" +
      pad2zero(cleanBankBin.length + cleanBankAccount.length + 38) +
      "0010A000000727" +
      "01" +
      pad2zero(cleanBankBin.length + cleanBankAccount.length + 8) +
      "00" +
      pad2zero(cleanBankBin.length) +
      cleanBankBin +
      "01" +
      pad2zero(cleanBankAccount.length) +
      cleanBankAccount;
  v += "0208QRIBFTTA";
  v += "5303704"; // Tag 53: Transaction Currency (704 = VND)
  
  if (amount) {
      const amountStr = amount.toString();
      v += "54" + pad2zero(amountStr.length) + amountStr; // Tag 54: Transaction Amount
  }
  
  v += "5802VN"; // Tag 58: Country Code (VN)
  
  if (cleanOrderCode) {
      v += "62" + pad2zero(cleanOrderCode.length + 4) + "08" + pad2zero(cleanOrderCode.length) + cleanOrderCode; // Tag 62: Additional Data Field (Subtag 08: Reference Label)
  }
  
  v += "6304"; // Tag 63: CRC16 Checksum prefix
  v += crc16(v);
  
  return v;
}

