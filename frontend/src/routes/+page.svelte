<script>
    import { enrollExaminee } from "$lib/api/enrollment";

    const MAX_FILE_SIZE = 5 * 1024 * 1024;

    /** @type {'idle' | 'loading' | 'success' | 'error'} */
    let status = $state("idle");
    let selectedFile = $state(/** @type {File | null} */ (null));
    let successMessage = $state("");
    let errorMessage = $state("");

    const statusMessage = $derived.by(() => {
        if (status === "success") return successMessage;
        if (status === "error") return errorMessage;
        return "";
    });

    function resetMessages() {
        successMessage = "";
        errorMessage = "";
        status = "idle";
    }

    /**
     * @param {Event} event
     */
    function handleFileChange(event) {
        const input = /** @type {HTMLInputElement} */ (event.currentTarget);
        const file = input.files?.[0];

        resetMessages();
        selectedFile = null;

        if (!file) {
            return;
        }

        if (!file.type.startsWith("image/")) {
            status = "error";
            errorMessage =
                "Please upload an image file containing your National ID QR code.";
            input.value = "";
            return;
        }

        if (file.size > MAX_FILE_SIZE) {
            status = "error";
            errorMessage =
                "File is too large. Please upload an image under 5MB.";
            input.value = "";
            return;
        }

        selectedFile = file;
    }

    /**
     * @param {SubmitEvent} event
     */
    async function handleSubmit(event) {
        event.preventDefault();

        if (!selectedFile) {
            status = "error";
            errorMessage =
                "Please select your National ID QR image before submitting.";
            return;
        }

        status = "loading";
        successMessage = "";
        errorMessage = "";

        try {
            const result = await enrollExaminee(selectedFile);
            if (result.success) {
                status = "success";
                successMessage = result.message;
                return;
            }

            status = "error";
            errorMessage = result.error;
        } catch {
            status = "error";
            errorMessage =
                "Unable to connect to the server. Please try again later.";
        }
    }
</script>

<svelte:head>
    <title>VeriTest Enrollment Portal</title>
    <meta
        name="description"
        content="Upload your National ID QR code for VeriTest enrollment verification."
    />
</svelte:head>

<main class="mx-auto min-h-screen max-w-xl px-4 py-10 sm:px-6">
    <section
        class="rounded-xl border border-slate-200 bg-white p-6 shadow-sm sm:p-8"
    >
        <h1 class="text-2xl font-semibold tracking-tight text-slate-900">
            VeriTest Enrollment Portal
        </h1>
        <p class="mt-2 text-sm text-slate-600">
            Upload the QR image from your National ID to verify your eligibility
            for exam day access.
        </p>
        <p class="mt-4 rounded-md bg-blue-50 px-3 py-2 text-xs text-blue-800">
            Your National ID image is used solely for verification and securely
            stored.
        </p>

        <form class="mt-6 space-y-4" onsubmit={handleSubmit}>
            <div>
                <label
                    for="national-id-qr"
                    class="mb-2 block text-sm font-medium text-slate-800"
                >
                    National ID QR image
                </label>
                <input
                    id="national-id-qr"
                    name="national-id-qr"
                    type="file"
                    accept="image/*"
                    class="block w-full cursor-pointer rounded-md border border-slate-300 bg-white p-2 text-sm file:mr-3 file:rounded file:border-0 file:bg-slate-100 file:px-3 file:py-1.5 file:text-sm file:font-medium hover:file:bg-slate-200"
                    onchange={handleFileChange}
                    required
                />
                <p class="mt-1 text-xs text-slate-500">
                    Accepted formats: image files up to 5MB.
                </p>
            </div>

            <button
                type="submit"
                disabled={status === "loading"}
                class="inline-flex w-full items-center justify-center rounded-md bg-slate-900 px-4 py-2 text-sm font-medium text-white transition hover:bg-slate-700 disabled:cursor-not-allowed disabled:opacity-70"
            >
                {#if status === "loading"}
                    Verifying...
                {:else}
                    Submit Enrollment
                {/if}
            </button>
        </form>

        {#if status === "success"}
            <div
                class="mt-4 rounded-md border border-emerald-200 bg-emerald-50 px-3 py-2 text-sm text-emerald-800"
                role="status"
                aria-live="polite"
            >
                {statusMessage}
            </div>
        {/if}

        {#if status === "error"}
            <div
                class="mt-4 rounded-md border border-red-200 bg-red-50 px-3 py-2 text-sm text-red-800"
                role="alert"
                aria-live="assertive"
            >
                {statusMessage}
            </div>
        {/if}
    </section>
</main>
