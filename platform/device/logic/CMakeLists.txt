set(ECHO_LIBMAD_DIR ${PROJECT_SOURCE_DIR}/third_party/libmad)
set(FPM_64BIT ON)
set(SIZEOF_INT 4)
configure_file(${ECHO_LIBMAD_DIR}/mad.h.in ${CMAKE_CURRENT_BINARY_DIR}/libmad/mad.h @ONLY)
add_library(echo_libmad STATIC
    ${ECHO_LIBMAD_DIR}/bit.c
    ${ECHO_LIBMAD_DIR}/fixed.c
    ${ECHO_LIBMAD_DIR}/frame.c
    ${ECHO_LIBMAD_DIR}/huffman.c
    ${ECHO_LIBMAD_DIR}/layer12.c
    ${ECHO_LIBMAD_DIR}/layer3.c
    ${ECHO_LIBMAD_DIR}/stream.c
    ${ECHO_LIBMAD_DIR}/synth.c
    ${ECHO_LIBMAD_DIR}/timer.c
    ${ECHO_LIBMAD_DIR}/version.c
)
target_include_directories(echo_libmad SYSTEM
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/libmad
    PRIVATE ${ECHO_LIBMAD_DIR}
)
target_compile_definitions(echo_libmad PRIVATE OPT_ACCURACY NDEBUG)
target_compile_options(echo_libmad PRIVATE -w)
if(CMAKE_CROSSCOMPILING)
    target_include_directories(echo_libmad SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libmad_libc)
endif()

set(ECHO_HELIX_AAC_DIR ${PROJECT_SOURCE_DIR}/third_party/helix_aac)
add_library(echo_helix_aac STATIC
    ${ECHO_HELIX_AAC_DIR}/aacdec.c
    ${ECHO_HELIX_AAC_DIR}/aactabs.c
    ${ECHO_HELIX_AAC_DIR}/bitstream.c
    ${ECHO_HELIX_AAC_DIR}/buffers.c
    ${ECHO_HELIX_AAC_DIR}/dct4.c
    ${ECHO_HELIX_AAC_DIR}/decelmnt.c
    ${ECHO_HELIX_AAC_DIR}/dequant.c
    ${ECHO_HELIX_AAC_DIR}/fft.c
    ${ECHO_HELIX_AAC_DIR}/filefmt.c
    ${ECHO_HELIX_AAC_DIR}/huffman.c
    ${ECHO_HELIX_AAC_DIR}/hufftabs.c
    ${ECHO_HELIX_AAC_DIR}/imdct.c
    ${ECHO_HELIX_AAC_DIR}/noiseless.c
    ${ECHO_HELIX_AAC_DIR}/pns.c
    ${ECHO_HELIX_AAC_DIR}/stproc.c
    ${ECHO_HELIX_AAC_DIR}/tns.c
    ${ECHO_HELIX_AAC_DIR}/trigtabs.c
)
target_include_directories(echo_helix_aac SYSTEM
    PUBLIC ${ECHO_HELIX_AAC_DIR}
)
target_compile_definitions(echo_helix_aac PRIVATE NDEBUG USE_DEFAULT_STDLIB)
target_compile_options(echo_helix_aac PRIVATE -w)
if(CMAKE_CROSSCOMPILING)
    target_include_directories(echo_helix_aac SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libmad_libc)
endif()

set(ECHO_LIBOPUS_DIR ${PROJECT_SOURCE_DIR}/third_party/libopus)
add_library(echo_libopus STATIC
    ${ECHO_LIBOPUS_DIR}/src/opus.c
    ${ECHO_LIBOPUS_DIR}/src/opus_decoder.c
    ${ECHO_LIBOPUS_DIR}/src/extensions.c
    ${ECHO_LIBOPUS_DIR}/src/mapping_matrix.c
    ${ECHO_LIBOPUS_DIR}/src/opus_multistream_decoder.c
    ${ECHO_LIBOPUS_DIR}/celt/bands.c
    ${ECHO_LIBOPUS_DIR}/celt/celt.c
    ${ECHO_LIBOPUS_DIR}/celt/celt_decoder.c
    ${ECHO_LIBOPUS_DIR}/celt/cwrs.c
    ${ECHO_LIBOPUS_DIR}/celt/entcode.c
    ${ECHO_LIBOPUS_DIR}/celt/entdec.c
    ${ECHO_LIBOPUS_DIR}/celt/entenc.c
    ${ECHO_LIBOPUS_DIR}/celt/kiss_fft.c
    ${ECHO_LIBOPUS_DIR}/celt/laplace.c
    ${ECHO_LIBOPUS_DIR}/celt/mathops.c
    ${ECHO_LIBOPUS_DIR}/celt/mdct.c
    ${ECHO_LIBOPUS_DIR}/celt/modes.c
    ${ECHO_LIBOPUS_DIR}/celt/pitch.c
    ${ECHO_LIBOPUS_DIR}/celt/celt_lpc.c
    ${ECHO_LIBOPUS_DIR}/celt/quant_bands.c
    ${ECHO_LIBOPUS_DIR}/celt/rate.c
    ${ECHO_LIBOPUS_DIR}/celt/vq.c
    ${ECHO_LIBOPUS_DIR}/silk/CNG.c
    ${ECHO_LIBOPUS_DIR}/silk/code_signs.c
    ${ECHO_LIBOPUS_DIR}/silk/init_decoder.c
    ${ECHO_LIBOPUS_DIR}/silk/decode_core.c
    ${ECHO_LIBOPUS_DIR}/silk/decode_frame.c
    ${ECHO_LIBOPUS_DIR}/silk/decode_parameters.c
    ${ECHO_LIBOPUS_DIR}/silk/decode_indices.c
    ${ECHO_LIBOPUS_DIR}/silk/decode_pulses.c
    ${ECHO_LIBOPUS_DIR}/silk/decoder_set_fs.c
    ${ECHO_LIBOPUS_DIR}/silk/dec_API.c
    ${ECHO_LIBOPUS_DIR}/silk/gain_quant.c
    ${ECHO_LIBOPUS_DIR}/silk/interpolate.c
    ${ECHO_LIBOPUS_DIR}/silk/LP_variable_cutoff.c
    ${ECHO_LIBOPUS_DIR}/silk/NLSF_decode.c
    ${ECHO_LIBOPUS_DIR}/silk/NSQ.c
    ${ECHO_LIBOPUS_DIR}/silk/NSQ_del_dec.c
    ${ECHO_LIBOPUS_DIR}/silk/PLC.c
    ${ECHO_LIBOPUS_DIR}/silk/shell_coder.c
    ${ECHO_LIBOPUS_DIR}/silk/tables_gain.c
    ${ECHO_LIBOPUS_DIR}/silk/tables_LTP.c
    ${ECHO_LIBOPUS_DIR}/silk/tables_NLSF_CB_NB_MB.c
    ${ECHO_LIBOPUS_DIR}/silk/tables_NLSF_CB_WB.c
    ${ECHO_LIBOPUS_DIR}/silk/tables_other.c
    ${ECHO_LIBOPUS_DIR}/silk/tables_pitch_lag.c
    ${ECHO_LIBOPUS_DIR}/silk/tables_pulses_per_block.c
    ${ECHO_LIBOPUS_DIR}/silk/biquad_alt.c
    ${ECHO_LIBOPUS_DIR}/silk/bwexpander_32.c
    ${ECHO_LIBOPUS_DIR}/silk/bwexpander.c
    ${ECHO_LIBOPUS_DIR}/silk/debug.c
    ${ECHO_LIBOPUS_DIR}/silk/decode_pitch.c
    ${ECHO_LIBOPUS_DIR}/silk/inner_prod_aligned.c
    ${ECHO_LIBOPUS_DIR}/silk/lin2log.c
    ${ECHO_LIBOPUS_DIR}/silk/log2lin.c
    ${ECHO_LIBOPUS_DIR}/silk/LPC_analysis_filter.c
    ${ECHO_LIBOPUS_DIR}/silk/LPC_inv_pred_gain.c
    ${ECHO_LIBOPUS_DIR}/silk/table_LSF_cos.c
    ${ECHO_LIBOPUS_DIR}/silk/NLSF2A.c
    ${ECHO_LIBOPUS_DIR}/silk/NLSF_stabilize.c
    ${ECHO_LIBOPUS_DIR}/silk/NLSF_VQ_weights_laroia.c
    ${ECHO_LIBOPUS_DIR}/silk/pitch_est_tables.c
    ${ECHO_LIBOPUS_DIR}/silk/resampler.c
    ${ECHO_LIBOPUS_DIR}/silk/resampler_down2_3.c
    ${ECHO_LIBOPUS_DIR}/silk/resampler_down2.c
    ${ECHO_LIBOPUS_DIR}/silk/resampler_private_AR2.c
    ${ECHO_LIBOPUS_DIR}/silk/resampler_private_down_FIR.c
    ${ECHO_LIBOPUS_DIR}/silk/resampler_private_IIR_FIR.c
    ${ECHO_LIBOPUS_DIR}/silk/resampler_private_up2_HQ.c
    ${ECHO_LIBOPUS_DIR}/silk/resampler_rom.c
    ${ECHO_LIBOPUS_DIR}/silk/sigm_Q15.c
    ${ECHO_LIBOPUS_DIR}/silk/sort.c
    ${ECHO_LIBOPUS_DIR}/silk/sum_sqr_shift.c
    ${ECHO_LIBOPUS_DIR}/silk/stereo_decode_pred.c
    ${ECHO_LIBOPUS_DIR}/silk/stereo_MS_to_LR.c
    ${ECHO_LIBOPUS_DIR}/silk/stereo_find_predictor.c
    ${ECHO_LIBOPUS_DIR}/silk/stereo_quant_pred.c
    ${ECHO_LIBOPUS_DIR}/silk/LPC_fit.c
    ${ECHO_LIBOPUS_DIR}/silk/NLSF_unpack.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/vector_ops_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/schur64_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/schur_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/k2a_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/k2a_Q16_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/apply_sine_window_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/autocorr_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/burg_modified_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/pitch_analysis_core_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/residual_energy16_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/residual_energy_FIX.c
    ${ECHO_LIBOPUS_DIR}/silk/fixed/warped_autocorrelation_FIX.c
)
target_include_directories(echo_libopus SYSTEM
    PUBLIC
        ${ECHO_LIBOPUS_DIR}
        ${ECHO_LIBOPUS_DIR}/include
        ${ECHO_LIBOPUS_DIR}/celt
        ${ECHO_LIBOPUS_DIR}/silk
        ${ECHO_LIBOPUS_DIR}/silk/fixed
)
target_compile_definitions(echo_libopus PRIVATE HAVE_CONFIG_H)
target_compile_options(echo_libopus PRIVATE -w)
if(CMAKE_CROSSCOMPILING)
    target_include_directories(echo_libopus SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libmad_libc)
endif()

add_library(echo_device_logic STATIC
    aac_decoder.c
    aac_probe.c
    opus_decoder.c
    opus_probe.c
    audio_engine.c
    audio_math.c
    backlight.c
    battery_gauge.c
    boot_power.c
    watchdog.c
    bt_module.c
    card_write.c
    core1_aac.c
    core1_opus.c
    core1_control.c
    core1_mp3.c
    core1_stream.c
    dac_sequence.c
    flac_source.c
    dma_loop.c
    emmc_layout.c
    exfat.c
    exfat_create.c
    fat.c
    fat_create.c
    fat_layout.c
    fat_names.c
    fat_reserve.c
    flac_accel.c
    image_crc.c
    jack_sense.c
    key_ladder.c
    key_scanner.c
    mp3_decoder.c
    mp3_probe.c
    msc.c
    panel.c
    panic_text.c
    pll.c
    replay_gain_probe.c
    start_record.c
    sd_card.c
    stall_watch.c
    tick_clock.c
    updater.c
    usb_device.c
    usb_fallback.c
    wav_source.c
)
target_include_directories(echo_device_logic PUBLIC include)
if(CMAKE_CROSSCOMPILING)
    target_include_directories(echo_device_logic SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/libmad_libc)
endif()
target_link_libraries(echo_device_logic
    PUBLIC echo_core echo_hal echo_codec echo_libmad echo_helix_aac echo_libopus
    PRIVATE echo_options
)
