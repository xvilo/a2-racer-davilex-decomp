/* Requesters drawn over the dimmed menu: a yes/no question (reqvraag.cpp) and a text
 * input (reqinput.cpp). */
#ifndef A2_REQUESTERS_H
#define A2_REQUESTERS_H

/* "Ja"/"Nee": returns 1 for yes. */
int req_vraag(const char *fmt, ...);
/* Asks for a line of text into buf (size bytes); returns 1 for OK, 0 for cancel. */
int req_input(char *buf, int size, const char *fmt, ...);

#endif
