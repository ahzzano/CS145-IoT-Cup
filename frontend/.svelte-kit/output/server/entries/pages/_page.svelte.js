import { a1 as head, a2 as attr } from "../../chunks/renderer.js";
function _page($$renderer, $$props) {
  $$renderer.component(($$renderer2) => {
    let status = "idle";
    head("1uha8ag", $$renderer2, ($$renderer3) => {
      $$renderer3.title(($$renderer4) => {
        $$renderer4.push(`<title>VeriTest Enrollment Portal</title>`);
      });
      $$renderer3.push(`<meta name="description" content="Upload your National ID QR code for VeriTest enrollment verification."/>`);
    });
    $$renderer2.push(`<main class="mx-auto min-h-screen max-w-xl px-4 py-10 sm:px-6"><section class="rounded-xl border border-slate-200 bg-white p-6 shadow-sm sm:p-8"><h1 class="text-2xl font-semibold tracking-tight text-slate-900">VeriTest Enrollment Portal</h1> <p class="mt-2 text-sm text-slate-600">Upload the QR image from your National ID to verify your eligibility for exam day access.</p> <p class="mt-4 rounded-md bg-blue-50 px-3 py-2 text-xs text-blue-800">Your National ID image is used solely for verification and securely stored.</p> <form class="mt-6 space-y-4"><div><label for="national-id-qr" class="mb-2 block text-sm font-medium text-slate-800">National ID QR image</label> <input id="national-id-qr" name="national-id-qr" type="file" accept="image/*" class="block w-full cursor-pointer rounded-md border border-slate-300 bg-white p-2 text-sm file:mr-3 file:rounded file:border-0 file:bg-slate-100 file:px-3 file:py-1.5 file:text-sm file:font-medium hover:file:bg-slate-200" required=""/> <p class="mt-1 text-xs text-slate-500">Accepted formats: image files up to 5MB.</p></div> <button type="submit"${attr("disabled", status === "loading", true)} class="inline-flex w-full items-center justify-center rounded-md bg-slate-900 px-4 py-2 text-sm font-medium text-white transition hover:bg-slate-700 disabled:cursor-not-allowed disabled:opacity-70">`);
    {
      $$renderer2.push("<!--[-1-->");
      $$renderer2.push(`Submit Enrollment`);
    }
    $$renderer2.push(`<!--]--></button></form> `);
    {
      $$renderer2.push("<!--[-1-->");
    }
    $$renderer2.push(`<!--]--> `);
    {
      $$renderer2.push("<!--[-1-->");
    }
    $$renderer2.push(`<!--]--></section></main>`);
  });
}
export {
  _page as default
};
