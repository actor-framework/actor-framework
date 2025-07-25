#!/usr/bin/env groovy

@Library('caf-continuous-integration') _

// Configures the behavior of our stages.
config = [
    // Version dependency for the caf-continuous-integration library.
    ciLibVersion: 1.0,
    // GitHub path to repository.
    repository: 'actor-framework/actor-framework',
    // List of enabled checks for email notifications.
    checks: [
        'build',
        'tests',
    ],
    // Default CMake flags for the builds.
    buildFlags: [
        'CAF_ENABLE_EXAMPLES:BOOL=ON',
        'CAF_ENABLE_ROBOT_TESTS:BOOL=ON',
        'CAF_ENABLE_RUNTIME_CHECKS:BOOL=ON',
    ],
    // Our build matrix. Keys are the operating system labels and values are build configurations.
    // Note on no-maybe-uninitialized: some GCC versions have weird bugs that causes false positives.
    buildMatrix: [
        // Release builds.
        ['almalinux-9', [ // EOL: May 2027
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
            extraBuildFlags: [
                'CMAKE_CXX_FLAGS:STRING=-Werror -Wno-maybe-uninitialized',
            ],
        ]],
        ['alpinelinux-3.21', [ // EOL: November 2026
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
            extraBuildFlags: [
                'CMAKE_CXX_FLAGS:STRING=-Werror -Wno-uninitialized -Wno-maybe-uninitialized -Wno-array-bounds -Wno-free-nonheap-object',
            ],
        ]],
        ['debian-11', [ // EOL June 2026
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
            extraBuildFlags: [
                'CMAKE_CXX_FLAGS:STRING=-Werror -Wno-maybe-uninitialized -Wno-deprecated-declarations',
            ],
        ]],
        ['debian-12', [ // EOL June 2028
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
            extraBuildFlags: [
                'CMAKE_CXX_FLAGS:STRING=-Werror -Wno-maybe-uninitialized -Wno-array-bounds -Wno-free-nonheap-object',
            ],
        ]],
        ['fedora-41', [ // EOL November 2025
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
            extraBuildFlags: [
                'CMAKE_CXX_FLAGS:STRING=-Werror -Wno-maybe-uninitialized -Wno-uninitialized -Wno-array-bounds',
            ],
        ]],
        ['fedora-42', [ // EOL May 2026
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
            extraBuildFlags: [
                'CMAKE_CXX_FLAGS:STRING=-Werror -Wno-maybe-uninitialized -Wno-uninitialized -Wno-array-bounds',
                'CAF_CXX_VERSION:STRING=23',
                'CAF_USE_STD_FORMAT:BOOL=ON',
            ],
        ]],
        ['ubuntu-22.04', [ // April 2027
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
            extraBuildFlags: [
                'CMAKE_CXX_FLAGS:STRING=-Werror -Wno-maybe-uninitialized',
            ],
        ]],
        ['ubuntu-24.04', [ // April 2029
            numCores: 4,
            tags: ['docker'],
            builds: ['release'],
            extraBuildFlags: [
                'CMAKE_CXX_FLAGS:STRING=-Werror -Wno-maybe-uninitialized -Wno-array-bounds',
            ],
        ]],
        // Debug build with exceptions disabled.
        ['fedora-42:no-exceptions', [
            numCores: 4,
            tags: ['docker'],
            builds: ['debug'],
            extraBuildFlags: [
                'CAF_LOG_LEVEL:STRING=TRACE',
                'CAF_ENABLE_EXCEPTIONS:BOOL=OFF',
                'CMAKE_CXX_FLAGS:STRING=-Werror -fno-exceptions',
            ],
        ]],
        // Debug build for LeakSanitizer.
        ['fedora-42:leak-checks', [
            numCores: 4,
            tags: ['docker', 'LeakSanitizer'],
            builds: ['debug'],
            extraBuildFlags: [
                'BUILD_SHARED_LIBS:BOOL=OFF',
                'CAF_LOG_LEVEL:STRING=TRACE',
                'CAF_SANITIZERS:STRING=address',
            ],
            extraBuildEnv: [
                'ASAN_OPTIONS=detect_leaks=1',
            ],
        ]],
        // Debug build with static libs, UBSan and hardening flags.
        ['fedora-42:ub-checks', [
            numCores: 4,
            tags: ['docker', 'UBSanitizer'],
            builds: ['debug'],
            extraBuildFlags: [
                'BUILD_SHARED_LIBS:BOOL=OFF',
                'CAF_LOG_LEVEL:STRING=TRACE',
                'CAF_SANITIZERS:STRING=address,undefined',
                'CMAKE_CXX_FLAGS:STRING=-Werror',
            ],
            extraBuildEnv: [
                'CXXFLAGS=-fno-sanitize-recover=undefined -D_GLIBCXX_DEBUG',
                'LDFLAGS=-fno-sanitize-recover=undefined',
            ],
        ]],
    ],
]

// Declarative pipeline for triggering all stages.
pipeline {
    options {
        buildDiscarder(logRotator(numToKeepStr: '50', artifactNumToKeepStr: '3'))
    }
    agent {
        label 'master'
    }
    environment {
        PrettyJobBaseName = env.JOB_BASE_NAME.replace('%2F', '/')
        PrettyJobName = "CAF/$PrettyJobBaseName #${env.BUILD_NUMBER}"
    }
    stages {
        stage('Checkout') {
            steps {
                getSources(config)
            }
        }
        stage('Build') {
            steps {
                buildParallel(config)
            }
        }
        stage('Notify') {
            steps {
                collectResults(config, PrettyJobName)
            }
        }
    }
    post {
        failure {
            emailext(
                subject: "$PrettyJobName: " + config['checks'].collect{ "⛔️ ${it}" }.join(', '),
                recipientProviders: [culprits(), developers(), requestor(), upstreamDevelopers()],
                attachLog: true,
                compressLog: true,
                body: "Check console output at ${env.BUILD_URL} or see attached log.\n",
            )
            notifyAllChecks(config, 'failure', 'Failed due to earlier error')
        }
    }
}
