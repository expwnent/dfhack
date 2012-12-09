

/**
 * RepeatPolicy: tells the plugin which of the following to do:
 *  1) schedule repeated calls of the plugin, and if so how often
 *  2) cancel scheduled repeated calls
 *  3) neither
 */

X(none) //do not alter repetition policy
X(cancel) //make the currently repeating instance, if any, stop repeating
X(monthly) //rerun at the start of every month
X(yearly) //rerun at the start of every year
X(birthsAndMigrants) //rerun any time new dwarves join the fort

