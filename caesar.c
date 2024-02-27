#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "caesar.h"




/**
 * Exercise 1
 * this function encodes the string "plaintext" using the Caesar cipher
 * by shifting characters by "key" positions.
 * Hint: you can reuse the memory from input as the output has
 * the same length as the input.
 **/
char *encode(char *plaintext, int key) {
    int len = strlen(plaintext);

    for (int i=0; i < len; i++) {
        // CASE1: valid, just shift 
        if (isalnum(plaintext[i])) {

            if (islower(plaintext[i])) {
                plaintext[i] = ((plaintext[i] - 'a' + key) % 26 + 26) % 26 + 'a';
            }

            // uppercase characters
            if (isupper(plaintext[i])) {
                plaintext[i] = ((plaintext[i] - 'A' + key) % 26 + 26) % 26 + 'A';
            }

            // numbers
            if (isdigit(plaintext[i])) {
                plaintext[i] = ((plaintext[i] - '0' + key) % 10 + 10) % 10 + '0';
            }

        // CASE2: if char not alphanumeric break
        } else {
            return "ILLCHAR";
        }
  
    }
    
    return plaintext;
}


/**
 * Exercise 2
 * This function decodes the "ciphertext" string using the Caesar cipher
 * by shifting the characters back by "key" positions.
 **/
char *decode(char *ciphertext, int key) {
    int len = strlen(ciphertext);
    int keyForDigit = key % 10;
    for (int i=0; i < len; i++) {
        // CASE1: valid, just shift 
        if (isalnum(ciphertext[i])) {
            if (islower(ciphertext[i])) {
                ciphertext[i] = (ciphertext[i] - 'a' - key + 26) % 26 + 'a';
            }

            // uppercase characters
            if (isupper(ciphertext[i])) {
                ciphertext[i] = (ciphertext[i] - 'A' - key + 26) % 26 + 'A';
            }

            // numbers
            if (isdigit(ciphertext[i])) {
                
                ciphertext[i] = (ciphertext[i] - '0' - keyForDigit + 10) % 10 + '0';
                //                      48     - 48  - 
            }

        // CASE2: if char not alphanumeric break
        } else {
            
            return "ILLCHAR";
        }
  
    }
    
    return ciphertext;
}
