
#include "xs.hpp"

int main ()
{
    xs::context_t ctx;
    xs::socket_t rep (ctx, XS_REP);
    rep.bind ("inproc://a");
    xs::socket_t req (ctx, XS_REQ);
    req.connect ("inproc://a");

    char buf [32];
    req.send ("ABC", 3);
    rep.recv (buf, sizeof (buf));
    rep.send ("DEF", 3);
    req.recv (buf, sizeof (buf));

    return 0;
}
