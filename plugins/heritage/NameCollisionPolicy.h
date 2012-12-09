

/*
 * NameCollisionPolicy: what to do if two units have the same full name. Cumulative: later policies will also use the methods of earlier policies. Later policies are slower.
 **/

X(ignore) //ignore name collisions
X(randomUnused) //try picking both last names at random
X(randomSecond) //try setting second name at random, then try all second names
X(uniqueNames) //if possible, set both last names to be new

