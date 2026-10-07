#include <juce_core/juce_core.h>

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();

    int numFailures = 0;
    for (int resultIndex = 0; resultIndex < runner.getNumResults(); ++resultIndex)
    {
        numFailures += runner.getResult(resultIndex)->failures;
    }
    if (numFailures > 0)
    {
        return 1;
    }
    return 0;
}
