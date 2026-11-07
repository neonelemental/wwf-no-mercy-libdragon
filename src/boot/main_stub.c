/* libdragon's _start calls main. No code calls _start yet, so this main
 * never runs; it only lets the link take _start and what it needs. */
int main(void)
{
	for (;;) {
	}
}
