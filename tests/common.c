#include <check.h>
#include <common.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

START_TEST(test_topdomain_ok)
{
	fail_if(check_topdomain("foo.0123456789.qwertyuiop.asdfghjkl.zxcvbnm.com"));

	/* Not allowed to start with dot */
	fail_unless(check_topdomain(".foo.0123456789.qwertyuiop.asdfghjkl.zxcvbnm.com"));
}
END_TEST

START_TEST(test_topdomain_length)
{
	/* Test empty and too short */
	fail_unless(check_topdomain(""));
	fail_unless(check_topdomain("a"));
	fail_unless(check_topdomain(".a"));
	fail_unless(check_topdomain("a."));
	fail_unless(check_topdomain("ab"));
	fail_if(check_topdomain("a.b"));

	/* Test too long (over 128, need rest of space for data) */
	fail_unless(check_topdomain(
		"abcd12345.abcd12345.abcd12345.abcd12345.abcd12345."
		"abcd12345.abcd12345.abcd12345.abcd12345.abcd12345."
		"abcd12345.abcd12345.foo129xxx"));
	fail_if(check_topdomain(
		"abcd12345.abcd12345.abcd12345.abcd12345.abcd12345."
		"abcd12345.abcd12345.abcd12345.abcd12345.abcd12345."
		"abcd12345.abcd12345.foo128xx"));
}
END_TEST

START_TEST(test_topdomain_chunks)
{
	/* Must have at least one dot */
	fail_if(check_topdomain("abcde.gh"));
	fail_unless(check_topdomain("abcdefgh"));

	/* Not two consecutive dots */
	fail_unless(check_topdomain("abc..defgh"));

	/* Not end with a dots */
	fail_unless(check_topdomain("abc.defgh."));

	/* No chunk longer than 63 chars */
	fail_unless(check_topdomain("123456789012345678901234567890"
		"1234567890123456789012345678904444.com"));
	fail_if(check_topdomain("123456789012345678901234567890"
		"123456789012345678901234567890333.com"));
	fail_unless(check_topdomain("abc.123456789012345678901234567890"
		"1234567890123456789012345678904444.com"));
	fail_if(check_topdomain("abc.123456789012345678901234567890"
		"123456789012345678901234567890333.com"));
	fail_unless(check_topdomain("abc.123456789012345678901234567890"
		"1234567890123456789012345678904444"));
	fail_if(check_topdomain("abc.123456789012345678901234567890"
		"123456789012345678901234567890333"));
}
END_TEST

START_TEST(test_parse_format_ipv4)
{
	char *host = "192.168.2.10";
	char *formatted;
	struct sockaddr_storage addr;
	struct sockaddr_in *v4addr;
	int addr_len;

	addr_len = get_addr(host, 53, AF_INET, 0, &addr);
	fail_unless(addr_len == sizeof(struct sockaddr_in));

	v4addr = (struct sockaddr_in *) &addr;
	fail_unless(v4addr->sin_addr.s_addr == htonl(0xc0a8020a));
	fail_unless(v4addr->sin_port == htons(53));

	formatted = format_addr(&addr, addr_len);
	fail_if(strcmp(host, formatted));
}
END_TEST

START_TEST(test_parse_format_ipv4_listen_all)
{
	char *host = "0.0.0.0";
	char *formatted;
	struct sockaddr_storage addr;
	struct sockaddr_in *v4addr;
	int addr_len;

	addr_len = get_addr(NULL, 53, AF_INET, AI_PASSIVE, &addr);
	fail_unless(addr_len == sizeof(struct sockaddr_in));

	v4addr = (struct sockaddr_in *) &addr;
	fail_unless(v4addr->sin_addr.s_addr == htonl(0x00000000));
	fail_unless(v4addr->sin_port == htons(53));

	formatted = format_addr(&addr, addr_len);
	fail_if(strcmp(host, formatted));
}
END_TEST

START_TEST(test_parse_format_ipv6)
{
	char *host = "2001:0db8:0505:0::123:0abc";
	char *compact = "2001:db8:505::123:abc";
	unsigned char v6_bits[] = {
		0x20, 0x01, 0x0d, 0xb8, 0x05, 0x05, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x01, 0x23, 0x0a, 0xbc,
	};
	char *formatted;
	struct sockaddr_storage addr;
	struct sockaddr_in6 *v6addr;
	int addr_len;

	addr_len = get_addr(host, 53, AF_UNSPEC, 0, &addr);
	fail_unless(addr_len == sizeof(struct sockaddr_in6));

	v6addr = (struct sockaddr_in6 *) &addr;
	fail_if(memcmp(&v6addr->sin6_addr, v6_bits, sizeof(v6_bits)));
	fail_unless(v6addr->sin6_port == htons(53));

	formatted = format_addr(&addr, addr_len);
	fail_if(strcmp(compact, formatted));
}
END_TEST

START_TEST(test_parse_format_ipv4_mapped_ipv6)
{
	char *v4mapped = "::FFFF:192.168.2.10";
	char *host = "192.168.2.10";
	unsigned char v6_bits[] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0xff, 0xff, 0xc0, 0xa8, 0x02, 0x0a,
	};
	char *formatted;
	struct sockaddr_storage addr;
	struct sockaddr_in6 *v6addr;
	int addr_len;

	addr_len = get_addr(v4mapped, 53, AF_INET6, 0, &addr);
	fail_unless(addr_len == sizeof(struct sockaddr_in6));

	v6addr = (struct sockaddr_in6 *) &addr;
	fail_if(memcmp(&v6addr->sin6_addr, v6_bits, sizeof(v6_bits)));
	fail_unless(v6addr->sin6_port == htons(53));

	/* Format as IPv4 address */
	formatted = format_addr(&addr, addr_len);
	fail_if(strcmp(host, formatted));
}
END_TEST

TCase *
test_common_create_tests()
{
	TCase *tc;
	int sock;

	tc = tcase_create("Common");
	tcase_add_test(tc, test_topdomain_ok);
	tcase_add_test(tc, test_topdomain_length);
	tcase_add_test(tc, test_topdomain_chunks);
	tcase_add_test(tc, test_parse_format_ipv4);
	tcase_add_test(tc, test_parse_format_ipv4_listen_all);

	/* Tests require IPv6 support */
	sock = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (sock >= 0) {
		close(sock);
		tcase_add_test(tc, test_parse_format_ipv6);
		tcase_add_test(tc, test_parse_format_ipv4_mapped_ipv6);
	}
	return tc;
}
