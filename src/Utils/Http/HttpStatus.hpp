#pragma once

namespace httpstatus
{
	enum value
	{
		code_continue = 100,
		code_switching_protocol,
		code_processing,
		code_early_hints,

		code_ok = 200,
		code_created,
		code_accepted,
		code_non_authoritative_information,
		code_no_content,
		code_reset_content,
		code_patrial_conent,
		code_multi_status,
		code_im_used = 226,

		code_multiple_choise = 300,
		code_moved_permanently,
		code_found,
		code_see_other,
		code_not_modified,
		code_use_proxy,
		code_unused,
		code_temporary_redirect,
		code_permanent_redirect,

		code_bad_request = 400,
		code_unauthorized,
		code_payment_required,
		code_forbidden,
		code_not_found,
		code_method_not_allowed,
		code_not_acceptable,
		code_proxy_authentification_required,
		code_request_timeout,
		code_conflict,
		code_gone,
		code_length_required,
		code_precondition_failed,
		code_payload_to_large,
		code_uri_to_long,
		code_unsupported_media_type,
		code_requested_range_not_satisfiable,
		code_expectation_failed,
		code_im_a_teapot,
		code_authentication_timeout,
		code_misdirected_request = 421,
		code_unprocessable_entity,
		code_locked,
		code_failed_dependency,
		code_too_early,
		code_upgrade_required,
		code_precondition_required = 428,
		code_too_many_requests,
		code_request_header_fields_too_large = 431,
		code_retry_with = 449,
		code_unavailable_for_legal_reasons = 451,
		code_client_closed_request = 499,

		code_internal_server_error = 500,
		code_not_implemented,
		code_bad_gateway,
		code_service_unavailable,
		code_gateway_timeout,
		code_http_version_not_supported,
		code_variant_also_negotiates,
		code_insufficient_storage,
		code_loop_detected,
		code_bandwidth_limit_exceeded,
		code_not_extended,
		code_network_authentication_required,
		code_unknown_error = 520,
		code_webserver_is_down,
		code_connection_timed_out,
		code_origin_is_unreachable,
		code_timeout_occurred,
		code_ssl_handshake_failed,
		code_ssl_cert_invalid,
	};

	inline bool is_informational(value code)
	{
		return ((code >= 100) && (code < 200));
	};

	inline bool is_success(value code)
	{
		return ((code >= 200) && (code < 300));
	};

	inline bool is_redirect(value code)
	{
		return ((code >= 300) && (code < 400));
	};

	inline bool is_client_error(value code)
	{
		return ((code >= 400) && (code < 500));
	};

	inline bool is_server_error(value code)
	{
		return ((code >= 500) && (code < 600));
	};
};
