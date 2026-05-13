<script>
    import { onMount } from "svelte";
    import { enrollExaminee } from "$lib/api/enrollment";

    const MAX_FILE_SIZE = 5 * 1024 * 1024;

    /** @type {'idle' | 'loading' | 'success' | 'error'} */
    let status = $state("idle");
    /** @type {'camera' | 'upload'} */
    let inputMode = $state("camera");
    /** @type {'idle' | 'starting' | 'active' | 'captured' | 'error'} */
    let cameraStatus = $state("idle");
    let selectedFile = $state(/** @type {File | null} */ (null));
    let selectedFileName = $state("");
    let capturedPreviewUrl = $state("");
    let successMessage = $state("");
    let errorMessage = $state("");
    let videoElement = $state(/** @type {HTMLVideoElement | null} */ (null));

    let cameraStream = /** @type {MediaStream | null} */ (null);

    const statusMessage = $derived.by(() => {
        if (status === "success") return successMessage;
        if (status === "error") return errorMessage;
        return "";
    });

    const cameraMessage = $derived.by(() => {
        if (cameraStatus === "starting") return "Opening camera...";
        if (cameraStatus === "active")
            return "Align the QR code inside the frame, then capture.";
        if (cameraStatus === "captured") return "Image captured.";
        return "";
    });

    onMount(() => {
        return () => {
            stopCamera();
            clearCapturedPreview();
        };
    });

    function resetMessages() {
        successMessage = "";
        errorMessage = "";
        status = "idle";
    }

    function clearCapturedPreview() {
        if (capturedPreviewUrl) {
            URL.revokeObjectURL(capturedPreviewUrl);
            capturedPreviewUrl = "";
        }
    }

    function clearCapturedFile() {
        selectedFile = null;
        selectedFileName = "";
        clearCapturedPreview();
        if (cameraStatus === "captured") {
            cameraStatus = "idle";
        }
    }

    /**
     * @param {File} file
     */
    function validateQrImage(file, source = "scan") {
        if (!file.type.startsWith("image/")) {
            status = "error";
            errorMessage =
                source === "upload"
                    ? "Please upload an image containing your National ID QR code."
                    : "Please scan an image containing your National ID QR code.";
            return false;
        }

        if (file.size > MAX_FILE_SIZE) {
            status = "error";
            errorMessage =
                source === "upload"
                    ? "File is too large. Please upload an image under 5MB."
                    : "Captured image is too large. Please scan again closer to the QR code.";
            return false;
        }

        return true;
    }

    /**
     * @param {File} file
     */
    function setCapturedFile(file) {
        resetMessages();
        if (!validateQrImage(file)) return;

        clearCapturedPreview();
        selectedFile = file;
        selectedFileName = file.name;
        capturedPreviewUrl = URL.createObjectURL(file);
        cameraStatus = "captured";
    }

    /**
     * @param {'camera' | 'upload'} mode
     */
    function setInputMode(mode) {
        if (inputMode === mode) return;

        inputMode = mode;
        resetMessages();
        clearCapturedFile();
        stopCamera();
    }

    /**
     * @param {Event} event
     */
    function handleFileChange(event) {
        const input = /** @type {HTMLInputElement} */ (event.currentTarget);
        const file = input.files?.[0];

        resetMessages();
        clearCapturedFile();
        stopCamera();

        if (!file) {
            return;
        }

        if (!validateQrImage(file, "upload")) {
            input.value = "";
            return;
        }

        selectedFile = file;
        selectedFileName = file.name;
    }

    async function startCamera() {
        resetMessages();
        clearCapturedFile();

        if (!navigator.mediaDevices?.getUserMedia) {
            cameraStatus = "error";
            status = "error";
            errorMessage =
                "This browser cannot access a camera. Please use Chrome, Edge, or another camera-enabled browser.";
            return;
        }

        cameraStatus = "starting";

        try {
            stopCamera();
            const stream = await navigator.mediaDevices.getUserMedia({
                video: {
                    facingMode: "user",
                    width: { ideal: 1280 },
                    height: { ideal: 720 },
                },
                audio: false,
            });

            cameraStream = stream;

            if (!videoElement) {
                throw new Error("Camera preview is unavailable.");
            }

            videoElement.srcObject = stream;
            await videoElement.play();

            cameraStatus = "active";
        } catch {
            stopCamera();
            cameraStatus = "error";
            status = "error";
            errorMessage =
                "Unable to open the camera. Check browser permissions and try again.";
        }
    }

    function stopCamera() {
        if (cameraStream) {
            for (const track of cameraStream.getTracks()) {
                track.stop();
            }
            cameraStream = null;
        }

        if (videoElement) {
            videoElement.srcObject = null;
        }

        if (cameraStatus === "active" || cameraStatus === "starting") {
            cameraStatus = selectedFile ? "captured" : "idle";
        }
    }

    async function captureVisibleQr() {
        resetMessages();

        const file = await captureVideoFrame();
        if (!file) {
            status = "error";
            errorMessage =
                "Camera frame is not ready yet. Please wait a moment and try again.";
            return;
        }

        setCapturedFile(file);
        stopCamera();
    }

    /**
     * @returns {Promise<File | null>}
     */
    async function captureVideoFrame() {
        if (!videoElement || !videoElement.videoWidth) {
            return null;
        }

        const canvas = document.createElement("canvas");
        canvas.width = videoElement.videoWidth;
        canvas.height = videoElement.videoHeight;

        const context = canvas.getContext("2d");
        if (!context) return null;

        context.drawImage(videoElement, 0, 0, canvas.width, canvas.height);

        return await new Promise((resolve) => {
            canvas.toBlob(
                (blob) => {
                    if (!blob) {
                        resolve(null);
                        return;
                    }

                    resolve(
                        new File([blob], "national-id-qr-scan.jpg", {
                            type: blob.type || "image/jpeg",
                        }),
                    );
                },
                "image/jpeg",
                0.9,
            );
        });
    }

    /**
     * @param {SubmitEvent} event
     */
    async function handleSubmit(event) {
        event.preventDefault();

        if (!selectedFile) {
            status = "error";
            errorMessage =
                inputMode === "upload"
                    ? "Please upload your National ID QR image before submitting."
                    : "Please scan your National ID QR code before submitting.";
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
        content="Scan your National ID QR code for VeriTest enrollment verification."
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
            Scan or upload the QR code from your National ID to verify your eligibility
            for exam day access.
        </p>
        <p class="mt-4 rounded-md bg-blue-50 px-3 py-2 text-xs text-blue-800">
            Your National ID QR frame is used solely for verification and securely
            stored.
        </p>

        <form class="mt-6 space-y-4" onsubmit={handleSubmit}>
            <div class="grid grid-cols-2 rounded-md border border-slate-300 p-1">
                <button
                    type="button"
                    class={`rounded px-3 py-2 text-sm font-medium transition ${
                        inputMode === "camera"
                            ? "bg-slate-900 text-white"
                            : "text-slate-700 hover:bg-slate-50"
                    }`}
                    aria-pressed={inputMode === "camera"}
                    onclick={() => setInputMode("camera")}
                >
                    Camera
                </button>
                <button
                    type="button"
                    class={`rounded px-3 py-2 text-sm font-medium transition ${
                        inputMode === "upload"
                            ? "bg-slate-900 text-white"
                            : "text-slate-700 hover:bg-slate-50"
                    }`}
                    aria-pressed={inputMode === "upload"}
                    onclick={() => setInputMode("upload")}
                >
                    Upload
                </button>
            </div>

            <div>
                {#if inputMode === "camera"}
                    <div class="mb-2 flex items-center justify-between gap-3">
                        <h2 class="text-sm font-medium text-slate-800">
                            National ID QR scan
                        </h2>
                        {#if cameraStatus === "active"}
                            <span class="text-xs font-medium text-emerald-700">
                                Camera active
                            </span>
                        {/if}
                    </div>

                    <div
                        class="relative overflow-hidden rounded-md border border-slate-300 bg-slate-950"
                    >
                        {#if capturedPreviewUrl}
                            <img
                                src={capturedPreviewUrl}
                                alt="Captured National ID QR frame"
                                class="aspect-video w-full object-contain"
                            />
                        {:else}
                            <video
                                bind:this={videoElement}
                                class="aspect-video w-full object-cover"
                                autoplay
                                muted
                                playsinline
                            >
                                <track kind="captions" />
                            </video>
                            <div
                                class="pointer-events-none absolute inset-0 flex items-center justify-center"
                                aria-hidden="true"
                            >
                                <div
                                    class="relative aspect-square w-[46%] min-w-32 max-w-56"
                                >
                                    <span
                                        class="absolute left-0 top-0 h-10 w-10 border-l-4 border-t-4 border-white"
                                    ></span>
                                    <span
                                        class="absolute right-0 top-0 h-10 w-10 border-r-4 border-t-4 border-white"
                                    ></span>
                                    <span
                                        class="absolute bottom-0 left-0 h-10 w-10 border-b-4 border-l-4 border-white"
                                    ></span>
                                    <span
                                        class="absolute bottom-0 right-0 h-10 w-10 border-b-4 border-r-4 border-white"
                                    ></span>
                                    <span
                                        class="absolute inset-0 rounded-sm border border-white/35"
                                    ></span>
                                </div>
                            </div>
                        {/if}
                    </div>

                    {#if cameraMessage}
                        <p class="mt-2 text-xs text-slate-500">
                            {cameraMessage}
                        </p>
                    {/if}

                    <div class="mt-3 grid grid-cols-2 gap-2">
                        {#if cameraStatus === "active"}
                            <button
                                type="button"
                                class="rounded-md border border-slate-300 px-3 py-2 text-sm font-medium text-slate-800 transition hover:bg-slate-50"
                                onclick={captureVisibleQr}
                            >
                                Capture
                            </button>
                            <button
                                type="button"
                                class="rounded-md border border-slate-300 px-3 py-2 text-sm font-medium text-slate-800 transition hover:bg-slate-50"
                                onclick={stopCamera}
                            >
                                Stop
                            </button>
                        {:else}
                            <button
                                type="button"
                                class="rounded-md border border-slate-300 px-3 py-2 text-sm font-medium text-slate-800 transition hover:bg-slate-50 disabled:cursor-not-allowed disabled:opacity-70"
                                disabled={cameraStatus === "starting" || status === "loading"}
                                onclick={startCamera}
                            >
                                {selectedFile ? "Rescan" : "Start Camera"}
                            </button>
                            <button
                                type="button"
                                class="rounded-md border border-slate-300 px-3 py-2 text-sm font-medium text-slate-800 transition hover:bg-slate-50 disabled:cursor-not-allowed disabled:opacity-70"
                                disabled={!selectedFile || status === "loading"}
                                onclick={clearCapturedFile}
                            >
                                Clear
                            </button>
                        {/if}
                    </div>
                {:else}
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
                    />
                    <p class="mt-1 text-xs text-slate-500">
                        Accepted formats: image files up to 5MB.
                    </p>
                    {#if selectedFileName}
                        <p class="mt-2 text-xs text-slate-600">
                            Selected: {selectedFileName}
                        </p>
                    {/if}
                {/if}
            </div>

            <button
                type="submit"
                disabled={status === "loading" || !selectedFile}
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
