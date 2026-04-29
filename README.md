# CS145-IoT-Cup
# VeriTest Enrollment Frontend (Phase 0)

SvelteKit + Tailwind frontend for National ID QR enrollment verification.

## Setup

1. Install dependencies:
   - `npm install`
2. Copy `.env.example` to `.env` and set `VITE_API_BASE_URL`
3. Run locally:
   - `npm run dev`

## API Contract

- Endpoint: `POST {VITE_API_BASE_URL}/enroll`
- Body: `FormData` with key `national_id_qr` (image file)
- Success:
  - `{ "success": true, "message": "string", "examinee_id": "string" }`
- Error:
  - `{ "success": false, "error": "string" }`

Only `src/lib/api/enrollment.js` should change if backend details change.