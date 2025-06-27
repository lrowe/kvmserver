import {
  ImageMagick,
  initializeImageMagick,
  MagickFormat,
} from "npm:@imagemagick/magick-wasm@0.0.35";
const magick_wasm = Deno.readFileSync(
  // Must not be named .wasm to deno compile --include
  import.meta.dirname + "/target/magick.wasm.bin",
);
await initializeImageMagick(magick_wasm);

const jpegData = Deno.readFileSync(
  import.meta.dirname + "/target/deno_city.jpeg",
);

Deno.serve(async () => {
  const buffer = await new Promise<Uint8Array>((resolve) => {
    ImageMagick.read(jpegData, (img) => {
      img.format = MagickFormat.Png;
      img.write((data) => resolve(data));
    });
  });
  return new Response(buffer, {
    headers: { "Content-Type": "image/png" },
  });
});
