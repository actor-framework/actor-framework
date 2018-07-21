#!/usr/bin/env groovy

// Default CMake flags for most builds (except coverage).
defaultBuildFlags = [
    'CAF_MORE_WARNINGS:BOOL=yes',
    'CAF_ENABLE_RUNTIME_CHECKS:BOOL=yes',
    'CAF_NO_OPENCL:BOOL=yes',
    'OPENSSL_ROOT_DIR=/usr/local/opt/openssl',
    'OPENSSL_INCLUDE_DIR=/usr/local/opt/openssl/include',
]

// CMake flags for release builds.
releaseBuildFlags = defaultBuildFlags + [
]

// CMake flags for debug builds.
debugBuildFlags = defaultBuildFlags + [
    'CAF_ENABLE_RUNTIME_CHECKS:BOOL=yes',
    'CAF_ENABLE_ADDRESS_SANITIZER:BOOL=yes',
    'CAF_LOG_LEVEL:STRING=4',
]

// Our build matrix. The keys are the operating system labels and the values
// are lists of tool labels.
buildMatrix = [
    // Debug builds with ASAN + logging for various OS/compiler combinations.
    ['Linux', [
        builds: ['debug'],
        tools: ['gcc4.8', 'gcc4.9', 'gcc5', 'gcc6', 'gcc7', 'gcc8'],
        cmakeArgs: debugBuildFlags,
    ]],
    ['macOS', [
        builds: ['debug'],
        tools: ['clang'],
        cmakeArgs: debugBuildFlags,
    ]],
    // One release build per supported OS. FreeBSD and Windows have the least
    // testing outside Jenkins, so we also explicitly schedule debug builds.
    ['Linux', [
        builds: ['release'],
        tools: ['gcc8', 'clang'],
        cmakeArgs: releaseBuildFlags,
    ]],
    ['macOS', [
        builds: ['release'],
        tools: ['clang'],
        cmakeArgs: releaseBuildFlags,
    ]],
    ['FreeBSD', [
        builds: ['debug'], // no release build for now, because it takes 1h
        tools: ['clang'],
        cmakeArgs: debugBuildFlags,
    ]],
    ['Windows', [
        builds: ['debug', 'release'],
        tools: ['msvc'],
        cmakeArgs: [
            'CAF_BUILD_STATIC_ONLY:BOOL=yes',
            'CAF_ENABLE_RUNTIME_CHECKS:BOOL=yes',
            'CAF_NO_OPENCL:BOOL=yes',
        ],
    ]],
    // One Additional build for coverage reports.
    ['Linux', [
        builds: ['debug'],
        tools: ['gcc8 && gcovr'],
        extraSteps: ['coverageReport'],
        cmakeArgs: [
            'CAF_ENABLE_GCOV:BOOL=yes',
            'CAF_NO_EXCEPTIONS:BOOL=yes',
            'CAF_FORCE_NO_EXCEPTIONS:BOOL=yes',
        ],
    ]],
]

// Optional environment variables for combinations of labels.
buildEnvironments = [
    'macOS && gcc': ['CXX=g++'],
    'Linux && clang': ['CXX=clang++'],
]

// Called *after* a build succeeded.
def coverageReport() {
    dir('caf-sources') {
        sh 'gcovr -e examples -e tools -e libcaf_test -e ".*/test/.*" -e libcaf_core/caf/scheduler/profiled_coordinator.hpp -x -r .. > coverage.xml'
        archiveArtifacts '**/coverage.xml'
        cobertura([
            autoUpdateHealth: false,
            autoUpdateStability: false,
            coberturaReportFile: '**/coverage.xml',
            conditionalCoverageTargets: '70, 0, 0',
            failUnhealthy: false,
            failUnstable: false,
            lineCoverageTargets: '80, 0, 0',
            maxNumberOfBuilds: 0,
            methodCoverageTargets: '80, 0, 0',
            onlyStable: false,
            sourceEncoding: 'ASCII',
            zoomCoverageChart: false,
        ])
    }
}

def cmakeSteps(buildType, cmakeArgs, installDir) {
    def absInstallDir = pwd() + '/' + installDir
    dir('caf-sources') {
        // Configure and build.
        cmakeBuild([
            buildDir: 'build',
            buildType: buildType,
            cmakeArgs: "-DCMAKE_INSTALL_PREFIX=\"$absInstallDir\" " + cmakeArgs,
            installation: 'cmake in search path',
            sourceDir: '.',
            steps: [[
                args: '--target install',
                withCmake: true,
            ]],
        ])
        // Run unit tests.
        ctest([
            arguments: '--output-on-failure',
            installation: 'cmake in search path',
            workingDir: 'build',
        ])
    }
    // Only generate artifacts for the master branch.
    if (PrettyJobBaseName == 'master') {
      zip([
          archive: true,
          dir: installDir,
          zipFile: "${installDir}.zip",
      ])
    }
}

// Builds `name` with CMake and runs the unit tests.
def buildSteps(buildType, cmakeArgs, installDir) {
    echo "build stage: $STAGE_NAME"
    deleteDir()
    dir(installDir) {
        // Create directory.
    }
    unstash('caf-sources')
    if (STAGE_NAME.contains('Windows')) {
        echo "Windows build on $NODE_NAME"
        withEnv(['PATH=C:\\Windows\\System32;C:\\Program Files\\CMake\\bin;C:\\Program Files\\Git\\cmd;C:\\Program Files\\Git\\bin']) {
            cmakeSteps(buildType, cmakeArgs, installDir)
        }
    } else {
        echo "Unix build on $NODE_NAME"
        def leakCheck = STAGE_NAME.contains("Linux") && !STAGE_NAME.contains("clang")
        withEnv(["label_exp=" + STAGE_NAME.toLowerCase(),
                 "ASAN_OPTIONS=detect_leaks=" + (leakCheck ? 1 : 0)]) {
            cmakeSteps(buildType, cmakeArgs, installDir)
        }
    }
}

// Concatenates CMake flags into a single string.
def makeFlags(xs) {
    xs.collect { x -> '-D' + x }.join(' ')
}

// Builds a stage for given builds. Results in a parallel stage `if builds.size() > 1`.
def makeBuildStages(matrixIndex, builds, lblExpr, settings) {
    builds.collectEntries { buildType ->
        def id = "$matrixIndex $lblExpr: $buildType"
        [
            (id):
            {
                node(lblExpr) {
                    stage(id) {
                      try {
                          def installDir = "$lblExpr && $buildType"
                          withEnv(buildEnvironments[lblExpr] ?: []) {
                              buildSteps(buildType, makeFlags(settings['cmakeArgs']), installDir)
                              (settings['extraSteps'] ?: []).each { fun -> "$fun"() }
                          }
                      } finally {
                          cleanWs()
                      }
                    }
                }
            }
        ]
    }
}

pipeline {
    agent none
    environment {
        LD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
        DYLD_LIBRARY_PATH = "$WORKSPACE/caf-sources/build/lib"
        PrettyJobBaseName = env.JOB_BASE_NAME.replace('%2F', '/')
        PrettyJobName = "CAF build #${env.BUILD_NUMBER} for $PrettyJobBaseName"
    }
    stages {
        stage('Git Checkout') {
            agent { label 'master' }
            steps {
                deleteDir()
                dir('caf-sources') {
                    checkout scm
                }
                stash includes: 'caf-sources/**', name: 'caf-sources'
            }
        }
        // Start builds.
        stage('Builds') {
            steps {
                script {
                    // Create stages for building everything in our build matrix in
                    // parallel.
                    def xs = [:]
                    buildMatrix.eachWithIndex { entry, index ->
                        def (os, settings) = entry
                        settings['tools'].eachWithIndex { tool, toolIndex ->
                            def matrixIndex = "[$index:$toolIndex]"
                            def builds = settings['builds']
                            def labelExpr = "$os && $tool"
                            xs << makeBuildStages(matrixIndex, builds, labelExpr, settings)
                        }
                    }
                    parallel xs
                }
            }
        }
    }
    post {
        success {
            emailext(
                subject: "✅ $PrettyJobName succeeded",
                recipientProviders: [culprits(), developers(), requestor(), upstreamDevelopers()],
                body: "Check console output at ${env.BUILD_URL}.",
            )
        }
        failure {
            emailext(
                subject: "⛔️ $PrettyJobName failed",
                attachLog: true,
                compressLog: true,
                recipientProviders: [culprits(), developers(), requestor(), upstreamDevelopers()],
                body: "Check console output at ${env.BUILD_URL} or see attached log.\n",
            )
        }
    }
}

