#!/usr/bin/env groovy

@Library('caf-continuous-integration') _

// Default CMake flags for release builds.
defaultReleaseBuildFlags = [
    'CAF_ENABLE_RUNTIME_CHECKS:BOOL=yes',
    'CAF_NO_OPENCL:BOOL=yes',
    'CAF_INSTALL_UNIT_TESTS:BOOL=yes',
]

// Default CMake flags for debug builds.
defaultDebugBuildFlags = defaultReleaseBuildFlags + [
    'CAF_ENABLE_ADDRESS_SANITIZER:BOOL=yes',
    'CAF_LOG_LEVEL:STRING=TRACE',
]

// Configures the behavior of our stages.
config = [
    // GitHub path to repository.
    repository: 'actor-framework/actor-framework',
    // List of enabled checks for email notifications.
    checks: [
        'build',
        'style',
        'tests',
        'coverage',
    ],
    // Our build matrix. Keys are the operating system labels and values are build configurations.
    buildMatrix: [
        ['Linux', [
            builds: ['debug'],
            tools: ['gcc4.8', 'gcc4.9', 'gcc5', 'gcc6', 'gcc7'],
        ]],
        ['Linux', [
            builds: ['debug'],
            tools: ['gcc8'],
            extraSteps: ['coverageReport'],
        ]],
        ['Linux', [
            builds: ['release'],
            tools: ['gcc8', 'clang'],
        ]],
        ['macOS', [
            builds: ['debug', 'release'],
            tools: ['clang'],
        ]],
        ['FreeBSD', [
            builds: ['debug'], // no release build for now, because it takes 1h
            tools: ['clang'],
        ]],
        ['Windows', [
            // TODO: debug build currently broken
            //builds: ['debug', 'release'],
            builds: ['release'],
            tools: ['msvc'],
        ]],
    ],
    // Platform-specific environment settings.
    buildEnvironments: [
        nop: [], // Dummy value for getting the proper types.
    ],
    // Default CMake flags by build type.
    defaultBuildFlags: [
        debug: defaultDebugBuildFlags,
        release: defaultReleaseBuildFlags,
    ],
    // CMake flags by OS and build type to override defaults for individual builds.
    buildFlags: [
        macOS: [
            debug: defaultDebugBuildFlags + [
                'OPENSSL_ROOT_DIR=/usr/local/opt/openssl',
                'OPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include',
            ],
            release: defaultReleaseBuildFlags + [
                'OPENSSL_ROOT_DIR=/usr/local/opt/openssl',
                'OPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include',
            ],
        ],
        Windows: [
            debug: defaultDebugBuildFlags + [
                'CAF_BUILD_STATIC_ONLY:BOOL=yes',
            ],
            release: defaultReleaseBuildFlags + [
                'CAF_BUILD_STATIC_ONLY:BOOL=yes',
            ],
        ],
    ],
    // Configures what binary the coverage report uses and what paths to exclude.
    coverage: [
        binary: 'build/bin/caf-test',
        relativeExcludePaths: [
            'test',
        ],
    ],
]

// Declarative pipeline for triggering all stages.
pipeline {
    options {
        // Store no more than 50 logs and 10 artifacts.
        buildDiscarder(logRotator(numToKeepStr: '50', artifactNumToKeepStr: '3'))
    }
    agent {
        label 'master'
    }
    environment {
        PrettyJobBaseName = env.JOB_BASE_NAME.replace('%2F', '/')
        PrettyJobName = "CAF/$PrettyJobBaseName #${env.BUILD_NUMBER}"
        ASAN_OPTIONS = 'detect_leaks=0'
    }
    stages {
        stage('Checkout') {
            steps {
                notifyAllChecks(config, 'pending', '')
                getSources(config)
            }
        }
        stage('Lint') {
            agent { label 'clang-format' }
            steps {
                runClangFormat(config)
            }
        }
        stage('Build') {
            steps {
                buildParallel(config, PrettyJobBaseName)
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
