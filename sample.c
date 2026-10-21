int main() {
    int total = 0;
    int i = 1;

    for (i = 1; i <= 5; i++) {
        if (i < 4) {
            total = total + i;
        } else {
            total = total + 2;
        }
    }

    while (total > 20) {
        total = total - 1;
    }

    printf("Result:", total);
    return 0;
}
