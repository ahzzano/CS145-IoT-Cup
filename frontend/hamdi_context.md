# VeriTest - Enrollment Frontend Context

## 📖 Project Overview
- **Name:** VeriTest: Identity-Verified Examination Release and Collection (Team 12, CS145)
- **Goal:** Prevent licensure exam identity fraud via an IoT dispenser system that verifies examinees at enrollment, entry, and exit using National ID QR scanning, MOSIP authentication, and facial recognition.
- **Current Focus:** Phase 0 - Web-based Enrollment Portal

## 👤 My Role & Scope
- Build the **Enrollment Frontend** (Phase 0 only)
- **Tech Stack:** SvelteKit + Tailwind CSS
- **Backend:** Supabase (temporary), must be **fully adaptable** to teammate Will's custom backend later
- **Constraints:** Deployable, secure, database-agnostic UI, strict separation of concerns

## 🔄 Phase 0 Enrollment Workflow (Proposal Requirement)
1. Examinee accesses web portal & uploads National ID QR code image
2. Frontend sends upload to backend → Backend validates via MOSIP testbed
3a. ✅ Valid → Add to examinee roster/DB → Show success notification
3b. ❌ Invalid → Show error notification → Block enrollment

## 🌐 API Contract & Architecture
- **Service Layer Only:** All backend communication isolated in `src/lib/api/enrollment.js`
- **Endpoint:** `POST /enroll`
- **Request:** `FormData` with key `national_id_qr` (image file)
- **Success Response:** `{ "success": true, "message": "string", "examinee_id": "string" }`
- **Error Response:** `{ "success": false, "error": "string" }`
- **Environment Routing:** `VITE_API_BASE_URL` points to backend base URL (Supabase Edge Functions now → Will's server later)

## 📁 Expected File Structure
src/
├── lib/api/enrollment.js # Decoupled fetch wrapper (NO direct DB calls)
├── routes/+page.svelte # Main enrollment UI (Svelte 5 + Tailwind)
└── app.css # @tailwind base/components/utilities
.env # VITE_API_BASE_URL (local)
.env.example # VITE_API_BASE_URL=/api (commit)


## 💡 Development Guidelines for AI Assistance
- Use **Svelte 5** syntax (`$state`, `$derived`, `on:submit` or `onsubmit`)
- Maintain UI states: `idle | loading | success | error`
- Client-side validation: `accept="image/*"`, max size `5MB`
- Display accessible status banners (green success, red error)
- Include brief privacy notice: `"Your National ID image is used solely for verification and securely stored."`
- Mock backend during dev: replace `fetch` with `setTimeout` returning `{ success: true }`
- **Never** embed Supabase keys or DB logic in UI components

## 🚀 Deployment & Environment
- Deploy to **Vercel** or **Netlify** (SvelteKit auto-optimizes)
- Set `VITE_API_BASE_URL` in deployment dashboard
- Enforce HTTPS, disable local file caching for uploads
- Zero UI changes required when switching to Will's backend

## 🔁 Backend Adaptability Plan
1. Will exposes `POST /enroll` with identical payload/response shape
2. Update `VITE_API_BASE_URL` in deployment config
3. If API shape changes, modify **ONLY** `src/lib/api/enrollment.js`
4. Coordinate HTTP status codes (`200` success, `400` bad QR, `409` duplicate, `500` MOSIP fail) early