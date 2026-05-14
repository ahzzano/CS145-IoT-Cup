import jsQR from 'jsqr';

// const DEFAULT_API_BASE_URL = '/api';
const DEFAULT_API_BASE_URL = 'http://localhost:8000'

// ── QR helpers ──────────────────────────────────────
 
/**
 * Decodes a QR code from an image File/Blob.
 * @param {File | Blob} imageFile
 * @returns {Promise<string | null>}
 */
async function readQR(imageFile) {
    return new Promise((resolve) => {
        const img = new Image();
        const url = URL.createObjectURL(imageFile);
 
        img.onload = () => {
            URL.revokeObjectURL(url);
 
            const canvas = document.createElement('canvas');
            canvas.width  = img.naturalWidth;
            canvas.height = img.naturalHeight;
 
            const ctx = canvas.getContext('2d');
            // @ts-ignore
            ctx.drawImage(img, 0, 0);
 
            // @ts-ignore
            const { data, width, height } = ctx.getImageData(0, 0, canvas.width, canvas.height);
            const code = jsQR(data, width, height);
 
            resolve(code?.data ?? null);
        };
 
        img.onerror = () => {
            URL.revokeObjectURL(url);
            resolve(null);
        };
 
        img.src = url;
    });
}
 
/**
 * Parses the decoded QR string from a Philippine National ID.
 * @param {string} qrData
 * @returns {{ uin: string; name: string } | null}
 */
function parseNationalIdQR(qrData) {
    try {
        const { uin, name } = JSON.parse(qrData);
        if (!uin || !name) return null;
        return { uin, name };
    } catch {
        return null;
    }
}

/**
 * @typedef {{ success: true; message: string; examinee_id: string }} EnrollmentSuccess
 * @typedef {{ success: false; error: string }} EnrollmentError
 * @typedef {EnrollmentSuccess | EnrollmentError} EnrollmentResponse
 */

/**
 * @param {string} img_path
 * @returns {Promise<Blob | null>}
 */
async function getExamineeImage(img_path) {
    const apiBaseUrl = import.meta.env.VITE_API_BASE_URL || DEFAULT_API_BASE_URL;
    console.log(`${apiBaseUrl}/${img_path}`)
    const response = await fetch(`${apiBaseUrl}/${img_path}`)

    if (!response.ok) {
        return null
    } else {
        return await response.blob()
    }
}


/**
 * Sends QR image for enrollment validation and registration.
 * @param {File} qrImage
 * @returns {Promise<EnrollmentResponse>}
 */
export async function enrollExaminee(qrImage) {
    const apiBaseUrl = import.meta.env.VITE_API_BASE_URL || DEFAULT_API_BASE_URL;   
    const auth_form_data = new FormData();
    auth_form_data.append('file', qrImage);
    
    const qrData = await readQR(qrImage);
    if (!qrData) {
        return { success: false, error: 'No QR code detected in the provided image.' };
    }
    const parsed = parseNationalIdQR(qrData);
    console.log(parsed);
    if (!parsed) {
        return { success: false, error: 'QR code does not contain valid National ID data.' };
    }
    auth_form_data.append("name", parsed.name);
    auth_form_data.append("uin", parsed.uin);

    const auth_response = await fetch(`${apiBaseUrl}/mosip/auth`, {
        method: 'POST',
        body: auth_form_data,
        cache: 'no-store',
        credentials: 'include'
    });

    const auth_json = await auth_response.json().catch(() => ({}));

    if (!auth_response.ok) {
        return {
            success: false,
            error: auth_json?.error || 'Unable to enroll. Please try again.'
        };
    }

    const examinee_id = parsed.uin

    const existing_examinee_response = await fetch(
        `${apiBaseUrl}/examinee/?id=${encodeURIComponent(examinee_id)}`,
        {
            method: 'GET',
            cache: 'no-store',
            credentials: 'include'
        }
    )

    if (existing_examinee_response.ok) {
        return {
            success: false,
            error: 'User already enrolled'
        };
    }

    if (existing_examinee_response.status !== 404) {
        const existing_examinee_json = await existing_examinee_response.json().catch(() => ({}))
        return {
            success: false,
            error: existing_examinee_json?.error || 'Unable to check enrollment status.'
        };
    }

    const kyc_response = await fetch(`${apiBaseUrl}/mosip/kyc`, {
        method: 'GET',
        cache: 'no-store',
        credentials: 'include'
    })

    if (!kyc_response.ok) {
        return {
            success: false,
            error: auth_json?.error || 'Unable to enroll. Please try again.'
        };
    }

    const kyc_json = await kyc_response.json().catch(() => ({}))
    const examinee_picture = kyc_json.data.photo_path
    const examinee_name = kyc_json.data.name

    const examinee_picture_fname = examinee_picture.split("/")[1]
    console.log(examinee_picture_fname)

    const picture_blob = await getExamineeImage(examinee_picture)
    if (!picture_blob) {
        return {
            success: false,
            error: 'Unable to retrieve the verified examinee photo.'
        };
    }

    const picture_file = new File([picture_blob], examinee_picture_fname, {type: picture_blob.type})

    const examinee_form_data = new FormData()
    examinee_form_data.append('file', picture_file)
    examinee_form_data.append('name', examinee_name)

    const new_examinee_response = await fetch(`${apiBaseUrl}/examinee/`, {
        method: 'POST',
        body: examinee_form_data,
        cache: 'no-store',
        credentials: 'include'
    })

    const new_examinee_json = await new_examinee_response.json().catch(() => ({}))

    if(!new_examinee_response.ok) {
        return {
            success: false,
            error: new_examinee_json?.error || 'Unable to enroll. Please try again.'
        };
    }

    return {
        success: true,
        message: 'Enrollment successful.',
        examinee_id: auth_json.examinee_id || ''
    };
}
