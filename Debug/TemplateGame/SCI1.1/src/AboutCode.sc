// Displays a message in response to a call to :func:`showAbout`.
(version 2)
(include "sci.sh")
(include "game.sh")
(include "13.shm")
(exports
    0 aboutCode
)
(use "Main")
(use "System")
(script ABOUT_SCRIPT)



(instance public aboutCode of Code
    (properties)
    (method (doit)
        (send gMessager:say(N_MAIN 0 0 0 0 13))
    )

)
