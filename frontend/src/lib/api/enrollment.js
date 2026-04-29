const DEFAULT_API_BASE_URL = '/api';

/**
 * @typedef {{ success: true; message: string; examinee_id: string }} EnrollmentSuccess
 * @typedef {{ success: false; error: string }} EnrollmentError
 * @typedef {EnrollmentSuccess | EnrollmentError} EnrollmentResponse
 */

/**
 * Sends QR image for enrollment validation and registration.
 * @param {File} qrImage
 * @returns {Promise<EnrollmentResponse>}
 */
export async function enrollExaminee(qrImage) {
  const apiBaseUrl = import.meta.env.VITE_API_BASE_URL || DEFAULT_API_BASE_URL;
  const formData = new FormData();
  formData.append('national_id_qr', qrImage);

  const response = await fetch(`${apiBaseUrl}/enroll`, {
    method: 'POST',
    body: formData,
    cache: 'no-store'
  });

  const payload = await response.json().catch(() => ({}));

  if (!response.ok) {
    return {
      success: false,
      error: payload?.error || 'Unable to enroll. Please try again.'
    };
  }

  if (payload?.success) {
    return {
      success: true,
      message: payload.message || 'Enrollment successful.',
      examinee_id: payload.examinee_id || ''
    };
  }

  return {
    success: false,
    error: payload?.error || 'Enrollment failed. National ID QR may be invalid.'
  };
}
