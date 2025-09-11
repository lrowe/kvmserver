import {
  ImageMagick,
  initializeImageMagick,
  MagickFormat,
} from "@imagemagick/magick-wasm";
import magick_wasm from "@imagemagick/magick-wasm/magick.wasm" with {
  type: "bytes",
};
import jpegData from "./target/deno_city.jpeg" with { type: "bytes" };
await initializeImageMagick(magick_wasm);

Deno.serve(async () => {
  const buffer = await new Promise<Uint8Array<ArrayBuffer>>((resolve) => {
    ImageMagick.read(jpegData, (img) => {
      img.format = MagickFormat.Png;
      img.write((data) => resolve(data as Uint8Array<ArrayBuffer>));
    });
  });
  return new Response(buffer, {
    headers: { "Content-Type": "image/png" },
  });
});
