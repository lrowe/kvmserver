delete globalThis.performance;
let seed = 1234567890;
Math.random = () => {
  seed = (1103515245 * seed + 12345) % 2147483648;
  return seed / 2147483648; // Normalize to a value between 0 and 1
};
