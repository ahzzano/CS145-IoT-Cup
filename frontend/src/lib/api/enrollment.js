import { PUBLIC_BYPASS, PUBLIC_DEFAULT_API_BASE_URL } from '$env/static/public'
// const DEFAULT_API_BASE_URL = '/api';
// const DEFAULT_API_BASE_URL = 'http://localhost:8000'
// const DEFAULT_API_BASE_URL = 'https://veritest.duckdns.org'
const DEFAULT_API_BASE_URL = PUBLIC_DEFAULT_API_BASE_URL
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
    const response = await fetch(`${apiBaseUrl}/${img_path}`, {
        credentials: 'include'
    })

    if (!response.ok) {
        return null
    } else {
        return await response.blob()
    }
}

async function getKycData(examinee_uin, examinee_name) {
    const apiBaseUrl = import.meta.env.VITE_API_BASE_URL || DEFAULT_API_BASE_URL;   

    if(PUBLIC_BYPASS == 'true') {
        console.log("BYPASS HIT")
        const picture_blob = await fetch(`${apiBaseUrl}/serve/${examinee_uin}.jpg`, {
            credentials: 'include'
        })
        return {
            success: true,
            uin: examinee_uin,
            name: examinee_name,
            picture: `${examinee_uin}.jpg`,
            blob: await picture_blob.blob()
        }
    }
    const kyc_response = await fetch(`${apiBaseUrl}/mosip/kyc`, {
        method: 'GET',
        cache: 'no-store',
        credentials: 'include'
    })

    if (!kyc_response.ok) {
        return {
            success: false,
            error: 'Unable to enroll. Please try again.'
        };
    }

    const kyc_json = await kyc_response.json().catch(() => ({}))

    const pic = kyc_json.data.photo_path
    const name= kyc_json.data.name
    const uin = kyc_json.data.uin

    let examinee_picture_fname = pic.replace('serve/', '')
    console.log(examinee_picture_fname)

    let picture_blob = await getExamineeImage(pic)

    if(!picture_blob) {
        return {
            success: false, 
            error: 'Unable to retrieve enrollee picture'
        }
    }
    
    return {
        success: true,
        uin: uin,
        name: name,
        picture: examinee_picture_fname,
        blob: picture_blob
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

    console.log('authing...')
    const auth_response = await fetch(`${apiBaseUrl}/mosip/auth`, {
        method: 'POST',
        body: auth_form_data,
        cache: 'no-store',
        credentials: 'include'
    });
    console.log('auth done')

    const auth_json = await auth_response.json().catch(() => ({}));

    if (!auth_response.ok) {
        return {
            success: false,
            error: auth_json?.error || 'Unable to enroll. Please try again.'
        };
    }

    const examinee_id = auth_json.data.uin
    const examinee_name = auth_json.data.name
    const token = auth_json.data.token

    document.cookie = `token=${token}; path=/; max-age=${60 * 60 * 24 * 7}; SameSite=Strict`;

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

    console.log('kyc-ing...')
    const kyc = await getKycData(examinee_id, examinee_name)
    if (kyc.error) {
        return {
            success: false,
            error: kyc.error
        };
    }
    const picture_blob = kyc.blob
    const examinee_picture_fname = kyc.picture

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
