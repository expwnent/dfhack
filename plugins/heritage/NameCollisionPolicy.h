

/*
 * NameCollisionPolicy: what to do if two units have the same full name. Cumulative: later policies will also use the methods of earlier policies. Later policies are slower.
 **/

X(ignore) //ignore name collisions
X(noDoubles) //just prevent the second name from being the same as the first
X(randomNames) //try setting both names at random
X(randomSecond) //try setting second name at random, then try all second names
X(randomUnused) //try picking both last names at random
X(uniqueNames) //if possible, set both last names to be new

